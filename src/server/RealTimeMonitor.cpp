#include "RealTimeMonitor.h"
#include "INIReader.h"
#include <sys/inotify.h>
#include <unistd.h>
#include <map>
#include <syslog.h>
#include <spdlog/spdlog.h>

#include "ScanQueue.h" //악성코드 팀 추가

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
void RealTimeMonitor::printErrorAndExit(const std::string& msg) 
{
    std::cerr << msg << ": " << std::strerror(errno) << std::endl;
    std::exit(EXIT_FAILURE);
}

//FIMConfig.ini 파싱하는 함수
std::vector<std::pair<std::string, uint64_t>> parsePathsFromIni(const std::string& iniPath) 
{
    std::vector<std::pair<std::string, uint64_t>> pathAndMaskList;
    INIReader reader(iniPath);

    
    std::vector<std::string> sections = reader.Sections(); // vector로 섹션 리스트 받아오기

    for (const auto& section : sections) {


        std::string path = reader.Get(section, "Path", "NOT_FOUND");
        std::string events = reader.Get(section, "Events", "NOT_FOUND");


        if (path.empty() || events.empty()) {
            std::cerr << "⚠️ 섹션 [" << section << "]에 Path 또는 Events가 없습니다" << std::endl;
            continue;
        }

        uint64_t mask = parseCustomEventMask(events);
        pathAndMaskList.emplace_back(path, mask);

    }

    return pathAndMaskList;
}

//ini파일에서 읽어온 이벤트 유형을 사용자 정의 마스크(customMask)로 변경하는 함수 (감지된 이벤트와 비교하기 위해)
uint64_t parseCustomEventMask(const std::string& eventsStr) 
{
    std::unordered_map<std::string, CustomEvent> eventMap = 
    {
        {"CREATE",  CustomEvent::CREATE},
        {"DELETE",  CustomEvent::DELETE},
        {"MODIFY",  CustomEvent::MODIFY},
        {"ATTRIB",  CustomEvent::ATTRIB}
    };

    std::istringstream ss(eventsStr);
    std::string token;
    uint64_t mask = 0;

    while (std::getline(ss, token, '|')) 
    {
        token.erase(0, token.find_first_not_of(" \t"));// 공백 제거
        token.erase(token.find_last_not_of(" \t") + 1);

        std::transform(token.begin(), token.end(), token.begin(), ::toupper); // 대문자로 통일 (필요하면)

        auto it = eventMap.find(token);
        if (it != eventMap.end()) {
            mask |= it->second;
        } else {
            std::cerr << " 알 수 없는 이벤트: '" << token << "'\n";
        }
    }

    return mask;
}


//감지된 이벤트 유형을 사용자 정의 마스크(customMask) 변경 (사용자가 설정한 유형과 비교하기 위해)
uint64_t mapActualMaskToCustomMask(uint64_t actualMask) 
{
    uint64_t result = 0;

    if (actualMask & IN_CREATE)   result |= CREATE;
    if (actualMask & IN_DELETE)   result |= DELETE;
    if (actualMask & FAN_MODIFY)  result |= MODIFY;
    if (actualMask & FAN_ATTRIB)  result |= ATTRIB;

    return result;
}


// 경로별 사용자 정의 이벤트 마스크를 mUserEventFilters에 매핑
void RealTimeMonitor::AddWatchWithFilter(const std::string& path, uint64_t customMask) 
{
    if (std::find(mWatchDirs.begin(), mWatchDirs.end(), path) == mWatchDirs.end())
        mWatchDirs.push_back(path);

    mUserEventFilters[path] = customMask;
}

// 경로와 사용자 필터에 매핑된 디렉토리를 확인하고,
// 실제 발생한 마스크(actualMask)를 사용자 정의 마스크(customMask)로 변환한 뒤,
// 그 둘이 매칭되는지 검사해서, 이벤트 출력 여부를 결정
bool RealTimeMonitor::ShouldDisplayEvent(const std::string& path, uint64_t actualMask)
{
    uint64_t matched = mapActualMaskToCustomMask(actualMask);
    for (const auto& [dir, customMask] : mUserEventFilters)
    {
        if (path.find(dir) == 0)
        {
            if ((matched & customMask) != 0)
                return true;
        }
    }
    return false;
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

//fanotify 이벤트 처리 및 로그 출력
void RealTimeMonitor::processFanotifyEvents(struct fanotify_event_metadata* metadata, ssize_t bufLen) 
{
    while (FAN_EVENT_OK(metadata, bufLen)) 
    {
        if (metadata->vers != FANOTIFY_METADATA_VERSION) //fanotify 버전확인
        {
            std::cerr << "Mismatched fanotify metadata version" << std::endl;
            std::exit(EXIT_FAILURE);
        }

        auto fid = reinterpret_cast<struct fanotify_event_info_fid*>(metadata + 1); //FID 이벤트 타입인지 확인
        if (fid->hdr.info_type != FAN_EVENT_INFO_TYPE_FID)
        {
            std::cerr << "Unexpected event info type." << std::endl;
            std::exit(EXIT_FAILURE);
        }

        struct file_handle* fileHandle = reinterpret_cast<struct file_handle*>(fid->handle);
        int mountFd = findMountFdForFileHandle(fileHandle); //이벤트가 발생한 mountfd 찾기
        if (mountFd == -1) 
        {
            metadata = FAN_EVENT_NEXT(metadata, bufLen);
            continue;
        }

        int eventFd = open_by_handle_at(mountFd, fileHandle, O_RDONLY); //핸들로 찾은 mountfd 열기 
        if (eventFd == -1) 
        {
            metadata = FAN_EVENT_NEXT(metadata, bufLen);
            continue;
        }

        ssize_t linkLen = readlink( ("/proc/self/fd/" + std::to_string(eventFd)).c_str(), mPath.data(), PATH_MAX ); //핸들을 통해 찾은 mountfd 얻기
        if (linkLen == -1) 
        {
            close(eventFd);
            metadata = FAN_EVENT_NEXT(metadata, bufLen);
            continue;
        }
        mPath[linkLen] = '\0';

        if (ShouldDisplayEvent(mPath.data(), metadata->mask)) //필터링 후 출력
        {
            if (metadata->mask & FAN_MODIFY)
                std::cout << "📁 파일 수정 : " << mPath.data() << std::endl;
            if (metadata->mask & FAN_ATTRIB)
                std::cout << "📁 메타데이터 변경 : " << mPath.data() << std::endl;
        }

        close(eventFd);
        close(metadata->fd);
        metadata = FAN_EVENT_NEXT(metadata, bufLen);
    }
}

//Inotify 이벤트 처리 및 로그 출력
void RealTimeMonitor::processInotifyEvents() 
{
    char buffer[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
    ssize_t length = read(mInotifyFd, buffer, sizeof(buffer));
    if (length <= 0) return;

    for (char* ptr = buffer; ptr < buffer + length;) //이벤트 순회
    {
        struct inotify_event* event = (struct inotify_event*)ptr;
        std::string path = mInotifyWdToPath[event->wd] + "/" + event->name; //감시한 디렉토리 + 파일명 = 경로

        if (ShouldDisplayEvent(path, event->mask)) //필터링 후 출력
        {
            if (event->mask & IN_CREATE) 
            {
                std::cout << "📁 파일 생성 : " << path << std::endl;
                //syslog(LOG_INFO, "📁 파일 생성");
            }
            if (event->mask & IN_DELETE)
                std::cout << "📁 파일 삭제 : " << path << std::endl;
        }

        ptr += sizeof(struct inotify_event) + event->len;
    }
}

//fanotify, inotify 초기화 설정
bool RealTimeMonitor::Init() 
{
    mFanFd = fanotify_init(FAN_CLASS_NOTIF | FAN_REPORT_FID, O_RDONLY); // FAN_REPORT_FID: 파일 핸들(FID) 기반 이벤트 보고 모드 활성화
    mInotifyFd = inotify_init1(IN_NONBLOCK);                            // 이 옵션이 있어야 이벤트에서 핸들을 받아 open_by_handle_at()으로 fd 접근 가능하며,
    if (mFanFd == -1 || mInotifyFd == -1)                               // 그렇지 않으면 fd 직접 접근 시 오류가 발생할 수 있음
    {
        printErrorAndExit("fanotify/inotify init");
    }

    for (const auto& dir : mWatchDirs) //설정한 경로마다 mountfd 얻어서 벡터에 저장
    {
        int mountFd = open(dir.c_str(), O_DIRECTORY | O_RDONLY);
        if (mountFd == -1) 
        {
            printErrorAndExit(dir);
        }
        mMountFds.push_back(mountFd);

        
        int ret = fanotify_mark(mFanFd, FAN_MARK_ADD, // fanotify 설정
                                FAN_MODIFY | FAN_ATTRIB | FAN_EVENT_ON_CHILD,
                                AT_FDCWD, dir.c_str());
        if (ret == -1) printErrorAndExit("fanotify_mark");

       
        int wd = inotify_add_watch(mInotifyFd, dir.c_str(), IN_CREATE | IN_DELETE);  // inotify 설정
        if (wd == -1) printErrorAndExit("inotify_add_watch");
        mInotifyWdToPath[wd] = dir; // inotify가 반환한 watch descriptor(wd)를 실제 경로 문자열에 매핑 저장
    }
    return true;
}

//모니터링 시작 준비 확인 및 감시 경로 출력
void RealTimeMonitor::Start() 
{
    if (mFanFd == -1 || mInotifyFd == -1) 
    {
        std::cerr << "Init first!" << std::endl;
        return;
    }

    std::cout << "✅ 모니터링 시작" << std::endl;
    for (const auto& dir : mWatchDirs) 
    {
        std::cout << "- 감시 중 : " << dir << std::endl;
    }
     std::cout << std::endl;
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
        processFanotifyEvents(metadata, len);
    }

    if (fds[1].revents & POLLIN) //inotify 이벤트 처리
    {
        processInotifyEvents();
    }
}