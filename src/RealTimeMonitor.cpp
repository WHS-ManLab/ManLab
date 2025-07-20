#include "RealTimeMonitor.h"
#include "INIReader.h"
#include <sys/inotify.h>
#include <unistd.h>
#include <map>
#include <syslog.h>

// 생성자
RealTimeMonitor::RealTimeMonitor(const std::vector<std::string>& watchDirs)
    :  mFanFd(-1), mInotifyFd(-1), mWatchDirs(watchDirs) {}

// 소멸자
RealTimeMonitor::~RealTimeMonitor() 
{
    if (mFanFd > 0) close(mFanFd);
    if (mInotifyFd > 0) close(mInotifyFd);
    for (int fd : mMountFds) if (fd > 0) close(fd);
}

//에러 출력 함수
void RealTimeMonitor::printErrorAndExit(const std::string& msg, std::ostream& err) 
{
    std::cerr << msg << ": " << std::strerror(errno) << std::endl;
    std::exit(EXIT_FAILURE);
}

//FIMConfig.ini 파싱하는 함수
std::vector<std::pair<std::string, uint64_t>> parsePathsFromIni(const std::string& iniPath, std::ostream& err) 
{
    std::vector<std::pair<std::string, uint64_t>> pathAndMaskList;
    INIReader reader(iniPath);

    
    std::vector<std::string> sections = reader.Sections(); // vector로 섹션 리스트 받아오기

    for (const auto& section : sections) 
    {
        std::string path = reader.Get(section, "Path", "NOT_FOUND"); //경로 및 이벤트유형 파싱
        std::string events = reader.Get(section, "Events", "NOT_FOUND");


        if (path.empty() || events.empty()) {
            std::cerr << "⚠️ 섹션 [" << section << "]에 Path 또는 Events가 없습니다" << std::endl;
            continue;
        }

        uint64_t mask = parseCustomEventMask(events); //이벤트유형 custommask로 변경 
        pathAndMaskList.emplace_back(path, mask); 

    }

    return pathAndMaskList;
}

// excludeFilesByPath는 RealTimeMonitor 클래스 멤버라고 가정
void RealTimeMonitor::parseExcludeFromIni(const std::string& iniFilePath) {
    INIReader reader(iniFilePath);
    if (reader.ParseError() != 0) {
        return;
    }

    excludeFilesByPath.clear();

    for (int i = 1;; ++i) {
        std::string section = "TARGETS_" + std::to_string(i);
        if (!reader.HasSection(section)) break;

        std::string path = reader.Get(section, "Path", "");
        std::string excludeStr = reader.Get(section, "Exclude", "");

        std::unordered_set<std::string> excludeSet;
        std::stringstream ss(excludeStr);
        std::string file;
        while (std::getline(ss, file, ',')) {
            file.erase(std::remove_if(file.begin(), file.end(), ::isspace), file.end());
            if (!file.empty()) {
                excludeSet.insert(file);
            }
        }
        excludeFilesByPath[path] = excludeSet;
    }
}

bool RealTimeMonitor::IsExcludedFile(const std::string& fullPath) {
    std::string dir = fullPath.substr(0, fullPath.find_last_of('/'));
    std::string fileName = fullPath.substr(fullPath.find_last_of('/') + 1);

    auto it = excludeFilesByPath.find(dir);
    if (it == excludeFilesByPath.end()) return false;

    return it->second.find(fileName) != it->second.end();
}

//ini파일에서 읽어온 이벤트 유형을 사용자 정의 마스크(customMask)로 변경하는 함수 (감지된 이벤트와 비교하기 위해)
uint64_t parseCustomEventMask(const std::string& eventsStr, std::ostream& err) 
{
    std::unordered_map<std::string, CustomEvent> eventMap = 
    {
        {"CREATE",  CustomEvent::CREATE},
        {"DELETE",  CustomEvent::DELETE},
        {"MODIFY",  CustomEvent::MODIFY},
        {"ATTRIB",  CustomEvent::ATTRIB},
        {"RENAME",  CustomEvent::RENAME} // 추가
    };

    std::istringstream ss(eventsStr);
    std::string token;
    uint64_t mask = 0;

    while (std::getline(ss, token, '|')) 
    {
        token.erase(0, token.find_first_not_of(" \t"));// 공백 제거
        token.erase(token.find_last_not_of(" \t") + 1);

        std::transform(token.begin(), token.end(), token.begin(), ::toupper); // 대문자로 통일

        auto it = eventMap.find(token);
        if (it != eventMap.end()) 
        {
            mask |= it->second;
        } 
        else 
        {
            std::cerr << " 알 수 없는 이벤트: '" << token << "'\n";
        }
    }

    return mask;
}


//감지된 이벤트 유형을 사용자 정의 마스크(customMask) 변경 (사용자가 설정한 유형과 비교하기 위해)
uint64_t mapActualMaskToCustomMask(uint64_t actualMask) 
{
    uint64_t result = 0;

    if (actualMask & IN_CREATE)   
    {
        result |= CREATE;
    }
    if (actualMask & IN_DELETE)   
    {
        result |= DELETE;
    }
    if (actualMask & FAN_MODIFY)  
    {
        result |= MODIFY;
    }
    if (actualMask & FAN_ATTRIB)  
    {
        result |= ATTRIB;
    }
    if ((actualMask & IN_MOVED_FROM) || (actualMask & IN_MOVED_TO)) 
    {
        result |= RENAME;
    }

    return result;
}


// 경로별 사용자 정의 이벤트 마스크를 mUserEventFilters에 매핑
void RealTimeMonitor::AddWatchWithFilter(const std::string& path, uint64_t customMask) 
{
    if (std::find(mWatchDirs.begin(), mWatchDirs.end(), path) == mWatchDirs.end())
        mWatchDirs.push_back(path);

    mUserEventFilters[path] = customMask;
}

//inode 얻기
uint64_t RealTimeMonitor::getInodeFromFd(int fd) {
    struct stat statbuf;
    if (fstat(fd, &statbuf) == -1) {
        perror("fstat");
        return 0;  // 오류 시 0 반환 (처리 필요)
    }
    return statbuf.st_ino;
}

// 경로와 사용자 필터에 매핑된 디렉토리를 확인하고,
// 실제 발생한 마스크(actualMask)를 사용자 정의 마스크(customMask)로 변환한 뒤,
// 그 둘이 매칭되는지 검사해서, 이벤트 출력 여부를 결정
bool RealTimeMonitor::ShouldDisplayEvent(const std::string& path, uint64_t actualMask)
{
    uint64_t matched = mapActualMaskToCustomMask(actualMask);

    bool eventMatches = false;
    for (const auto& [dir, customMask] : mUserEventFilters)
    {
        // 경로가 완전히 같거나,
        // 경로가 dir로 시작하고 다음 문자가 '/'인 경우 (즉, 하위 경로)
        if (path == dir || (path.find(dir) == 0 && path[dir.size()] == '/'))
        {
            if ((matched & customMask) != 0)
            {
                eventMatches = true;
                break;
            }
        }
    }
    if (!eventMatches) {
        return false;
    }

    for (const auto& [exDir, excludeSet] : excludeFilesByPath)
    {
        if (path == exDir || (path.find(exDir) == 0 && path[exDir.size()] == '/'))
        {
            std::string fileName = path.substr(path.find_last_of('/') + 1);
            if (excludeSet.find(fileName) != excludeSet.end())
            {
                return false;
            }
        }
    }

    return true;
}

//파일 핸들이 속한 mountFD를 찾기
int RealTimeMonitor::findMountFdForFileHandle(const struct file_handle* fid) 
{
    for (size_t i = 0; i < mMountFds.size(); ++i) {
        int mountFd = mMountFds[i];
        int fd = open_by_handle_at(mountFd, const_cast<file_handle*>(fid), O_RDONLY);
        if (fd != -1) 
        {
            close(fd);
            return mountFd;
        }  
        else if (errno == ESTALE) 
        {
            continue;
        }
    }
    return -1;
}

//이벤트 중복 및 생성+메타데이터 중복 방지 함수
bool RealTimeMonitor::isDuplicateEvent(const std::string& path, uint64_t eventType) 
{
    using namespace std::chrono;
    auto now = steady_clock::now();

    auto it = mRecentEvents.find(path);

    if (it != mRecentEvents.end()) 
    {
        auto [lastType, lastTime] = it->second;

        // 1) 완전 중복 이벤트 필터
        if (lastType == eventType && duration_cast<milliseconds>(now - lastTime).count() < 50) 
        {
            return true;  // 바로 직전 같은 이벤트 중복
        }

        // 2) 생성(CREATE) 이벤트 직후 메타데이터 변경(ATTRIB) 이벤트 무시
        if (lastType == CREATE && eventType == ATTRIB && duration_cast<milliseconds>(now - lastTime).count() < 100)
        {
            return true;  // 생성 후 바로 이어지는 메타데이터 변경 무시
        }
    }

    // 중복이 아니면 기록 갱신
    mRecentEvents[path] = std::make_pair(eventType, now);
    return false;
}



//fanotify 이벤트 처리 및 로그 출력
void RealTimeMonitor::processFanotifyEvents(struct fanotify_event_metadata* metadata, ssize_t bufLen) 
{
    while (FAN_EVENT_OK(metadata, bufLen)) 
    {
        if (metadata->vers != FANOTIFY_METADATA_VERSION) 
        {
            std::cerr << "Mismatched fanotify metadata version" << std::endl;
            std::exit(EXIT_FAILURE);
        }

        auto fid = reinterpret_cast<struct fanotify_event_info_fid*>(metadata + 1);
        if (fid->hdr.info_type != FAN_EVENT_INFO_TYPE_FID)
        {
            std::cerr << "Unexpected event info type." << std::endl;
            std::exit(EXIT_FAILURE);
        }

        struct file_handle* fileHandle = reinterpret_cast<struct file_handle*>(fid->handle);
        int mountFd = findMountFdForFileHandle(fileHandle);
        if (mountFd == -1) 
        {
            metadata = FAN_EVENT_NEXT(metadata, bufLen);
            continue;
        }

        int eventFd = open_by_handle_at(mountFd, fileHandle, O_RDONLY); //핸들과 mountfd로 파일 열기
        if (eventFd == -1) 
        {
            metadata = FAN_EVENT_NEXT(metadata, bufLen);
            continue;
        }

        ssize_t linkLen = readlink( ("/proc/self/fd/" + std::to_string(eventFd)).c_str(), mPath.data(), PATH_MAX ); //eventFd를 통해 찾은 경로 얻기
        if (linkLen == -1) 
        {
            close(eventFd);
            metadata = FAN_EVENT_NEXT(metadata, bufLen);
            continue;
        }
        mPath[linkLen] = '\0';

     if (ShouldDisplayEvent(mPath.data(), metadata->mask)) 
    {
        if ((metadata->mask & FAN_MODIFY) && !isDuplicateEvent(mPath.data(), MODIFY))
        {
            std::cout << "📁 파일 수정 : " << mPath.data() << std::endl;
        }

        if ((metadata->mask & FAN_ATTRIB) && !isDuplicateEvent(mPath.data(), ATTRIB))
        {
            std::cout << "📁 메타데이터 변경 : " << mPath.data() << std::endl;
        }
        
    }


        close(eventFd);
        close(metadata->fd);
        metadata = FAN_EVENT_NEXT(metadata, bufLen);
    }
}

//Inotify 이벤트 처리 및 로그 출력
void RealTimeMonitor::processInotifyEvents() 
{
    char buffer[4096] __attribute__((aligned(__alignof__(struct inotify_event)))); //메모리 정렬
    ssize_t length = read(mInotifyFd, buffer, sizeof(buffer)); //데이터 읽어오기
    if (length <= 0) return;

    for (char* ptr = buffer; ptr < buffer + length;) // 이벤트 순회
{
    struct inotify_event* event = (struct inotify_event*)ptr;
    std::string path = mInotifyWdToPath[event->wd] + "/" + event->name;

    uint64_t mask = event->mask;

    // 생성 이벤트
    if ((mask & IN_CREATE) && ShouldDisplayEvent(path, IN_CREATE) && !isDuplicateEvent(path, CREATE))
    {
        std::cout << "📁 파일 생성 : " << path << std::endl;
    }
    // 삭제 이벤트
    if ((mask & IN_DELETE) && ShouldDisplayEvent(path, IN_DELETE) && !isDuplicateEvent(path, DELETE))
    {
        std::cout << "📁 파일 삭제 : " << path << std::endl;
    }
    // 리네임 이벤트 (추가한 경우)
    if ((mask & IN_MOVED_FROM) && ShouldDisplayEvent(path, IN_MOVED_FROM) && !isDuplicateEvent(path, RENAME))
    {
        std::cout << "📁 이름 변경(From) : " << path << std::endl;
    }
    if ((mask & IN_MOVED_TO) && ShouldDisplayEvent(path, IN_MOVED_TO) && !isDuplicateEvent(path, RENAME))
    {
        std::cout << "📁 이름 변경(To) : " << path << std::endl;
    }

    ptr += sizeof(struct inotify_event) + event->len;
}

}

//fanotify, inotify 초기화 설정
bool RealTimeMonitor::Init() 
{
    parseExcludeFromIni(PATH_FIM_CONFIG_INI);


    mFanFd = fanotify_init(FAN_CLASS_NOTIF | FAN_REPORT_FID, O_RDONLY); // FAN_REPORT_FID: 파일 핸들(FID) 기반 이벤트 보고 모드 활성화
    mInotifyFd = inotify_init1(IN_NONBLOCK);                            // 이 옵션이 있어야 이벤트에서 핸들을 받아 open_by_handle_at()으로 fd 접근 가능하며,
    if (mFanFd == -1 || mInotifyFd == -1)                               // 그렇지 않으면 fd 직접 접근 시 오류가 발생할 수 있음
    {
        printErrorAndExit("fanotify/inotify init", std::cerr );
    }

    for (const auto& dir : mWatchDirs) //설정한 경로마다 mountfd 얻어서 벡터에 저장
    {
        registerWatchPath(dir);
        int mountFd = open(dir.c_str(), O_DIRECTORY | O_RDONLY);
        if (mountFd == -1) 
        {
            printErrorAndExit(dir, std::cerr);
        }
        mMountFds.push_back(mountFd);

        
        int ret = fanotify_mark(mFanFd, FAN_MARK_ADD, // fanotify 설정
                                FAN_MODIFY | FAN_ATTRIB | FAN_CLOSE_WRITE | FAN_EVENT_ON_CHILD,
                                AT_FDCWD, dir.c_str());
        if (ret == -1) printErrorAndExit("fanotify_mark", std::cerr);

       
        int wd = inotify_add_watch(mInotifyFd, dir.c_str(), IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO);
        if (wd == -1) printErrorAndExit("inotify_add_watch");
        mInotifyWdToPath[wd] = dir; // inotify가 반환한 watch descriptor(wd)를 실제 경로 문자열에 매핑 저장
    }

    return true;
}

// fanotify와 inotify 파일 디스크립터에서 이벤트가 있는지  검사하고,
// 이벤트가 있으면 각각 처리 함수를 호출하여 이벤트를 처리한다.
void RealTimeMonitor::pollOnce() 
{
    struct pollfd fds[2];
    fds[0].fd = mFanFd;
    fds[0].events = POLLIN;
    fds[1].fd = mInotifyFd;
    fds[1].events = POLLIN;

    int ret = poll(fds, 2, 0);  // 타임아웃 0 ➜ 논블로킹
    if (ret == -1) 
    {
        perror("poll");
        return;
    }

    if (fds[0].revents & POLLIN) //fanotify 이벤트 처리
    {
        ssize_t len = read(mFanFd, mBuf.data(), mBuf.size());
        auto metadata = reinterpret_cast<struct fanotify_event_metadata*>(mBuf.data());
        processFanotifyEvents(metadata, len, std::cerr);
    }
    if (fds[1].revents & POLLIN) //inotify 이벤트 처리
    {
        processInotifyEvents();
    }
}
