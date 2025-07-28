#include "ScanWatchThread.h"
#include "INIReader.h"
#include "ScanQueue.h"
#include "StringUtils.h"
#include "Paths.h"

#include <unistd.h>
#include <dirent.h>
#include <sys/fanotify.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <string.h>
#include <poll.h>
#include <errno.h>
#include <future>
#include <filesystem>

namespace fs = std::filesystem;
using manlab::utils::trim;
using manlab::utils::stripComment;

ScanWatchThread::ScanWatchThread()
    : mpShouldRun(nullptr), 
      mFanFd(-1)
{}

ScanWatchThread::~ScanWatchThread()
{
    if (mFanFd >= 0) 
    {
        close(mFanFd);
    }
}

void ScanWatchThread::Init(std::atomic<bool>& shouldRun)
{
    mpShouldRun = &shouldRun;
    loadWatchConfig();
}

// 설정 파일을 파싱해 감시할 디렉토리 목록과 예외 목록을 구성
void ScanWatchThread::loadWatchConfig()
{
    INIReader reader(PATH_MALSCAN_CONFIG_INI);

    std::string scanPaths = reader.Get("Scan", "paths", "");
    std::stringstream ss(scanPaths);
    std::string token;
    while (std::getline(ss, token, ',')) 
    {
        token = trim(stripComment(token));
        if (!token.empty()) 
        {   
            mWatchDirs.push_back(token);
        }
    }

    std::string excludePaths = reader.Get("Exclude", "paths", "");
    std::stringstream ss2(excludePaths);
    while (std::getline(ss2, token, ',')) 
    {
        token = trim(stripComment(token));
        if (!token.empty()) 
        {
            mExcludeDirs.push_back(token);
        }
    }

    // max_size 읽기 (기본값 100MB)
    int maxMB = reader.GetInteger("Limit", "max_size", 100);
    if (maxMB <= 0) 
    {
        maxMB = 100;
    }
    mMaxScanSizeBytes = static_cast<std::uintmax_t>(maxMB) * 1024 * 1024;

    const std::vector<std::string> systemExcludes = 
    {
        "/proc", "/sys", "/dev", "/run",
        "/tmp", "/var/run", "/var/tmp",
        "/boot", "/mnt", "/media", "/lost+found",
        "/lib", "/lib64", "/usr/lib", "/usr/lib64",
        "/root/ManLab"  // ManLab 자체 디렉터리 (설정, 로그, 바이너리 등)
    };

    mExcludeDirs.insert(mExcludeDirs.end(), systemExcludes.begin(), systemExcludes.end());
}


void ScanWatchThread::Run()
{
    spdlog::info("ScanWatchThread: 실행 감지 감시 시작");

    //Fanotify 초기화
    if (!InitFanotify()) 
    {
        return;
    }
    spdlog::info("ScanWatchThread: 감시 등록 끝");

    // poll()에 사용할 pollfd 구조체 준비
    struct pollfd fds[1];
    fds[0].fd = mFanFd;
    fds[0].events = POLLIN;

    while (*mpShouldRun)
    {
        //poll()로 fds[0]을 감시, 최대 1초동안 대기
        int ret = poll(fds, 1, 1000);
        if (ret == 0) 
        {
            // 타임아웃(정상)
            continue;
        }
        if (ret < 0) 
        {
            spdlog::warn("poll 실패: {}", strerror(errno));
            continue;
        }

        // fanotify로부터 실행 요청 이벤트가 도착한 경우
        if (fds[0].revents & POLLIN) 
        {
            spdlog::info("실행 요청 이벤트 도착");
            processFanotifyEvents();
        }
    }

    spdlog::info("ScanWatchThread: 감시 종료");
}

// Fanotify 초기화
// 예외 디렉토리는 감시 대상에 포함되지 않도록 재귀 등록 옵션을 쓰지 않고 순회하며 추가
bool ScanWatchThread::InitFanotify()
{
    // FAN_CLASS_CONTENT: 실행 차단용 permission 이벤트를 받을 수 있게 함
    // FAN_NONBLOCK: read() 호출 시 블로킹되지 않도록 함(poll로 검사중이므로)
    mFanFd = fanotify_init(FAN_CLASS_CONTENT | FAN_NONBLOCK, O_RDONLY);
    if (mFanFd < 0) 
    {
        spdlog::error("fanotify 초기화 실패: {}", strerror(errno));
        return false;
    }

    if (mWatchDirs.empty())
    {
        spdlog::error("감시 대상 디렉토리가 비어 있습니다.");
        return false;
    }

    const std::string& rootPath = mWatchDirs.front();  // 오직 첫 번째 디렉토리만 사용

    // 스택 기반 탐색 : 디렉토리 순회하며 예외 디렉토리 배제
    // fnotify에 재귀 옵션을 주면 예외 디렉토리도 전부 포함되는 문제 발생
    std::vector<std::string> stack;
    stack.push_back(rootPath);

    while (!stack.empty())
    {
        std::string path = stack.back();
        stack.pop_back();

        // 예외 디렉토리 여부 확인
        bool isExcluded = false;
        for (const auto& ex : mExcludeDirs)
        {
            if (path == ex || path.find(ex + "/") == 0)
            {
                spdlog::debug("감시 제외됨 (예외 디렉토리): {}", path);
                isExcluded = true;
                break;
            }
        }

        if (isExcluded)
        {
            continue;
        }

        const uint64_t EVENT_MASK = FAN_OPEN_EXEC_PERM | FAN_EVENT_ON_CHILD; 
        // 현재 디렉토리 감시 등록
        // FAN_OPEN_EXEC_PERM: 실행 요청(permission) 이벤트 감지
        if (fanotify_mark(mFanFd,
                          FAN_MARK_ADD,
                          EVENT_MASK,
                          AT_FDCWD,
                          path.c_str()) < 0)
        {
            spdlog::warn("fanotify 마크 실패: {} - {}", path, strerror(errno));
            continue;
        }
        else
        {
            //spdlog::info("감시 등록: {}", path);
        }

        // 하위 디렉토리 탐색
        DIR* dir = opendir(path.c_str());
        if (!dir) 
        {
            continue;
        }

        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr)
        {
            std::string name = entry->d_name;
            if (name == "." || name == "..") 
            {
                continue;
            }

            std::string subdir = path + "/" + name;

            // symlink 확인
            struct stat sb;
            if (lstat(subdir.c_str(), &sb) == -1)
            {
                spdlog::warn("lstat 실패: {}", subdir);
                continue;
            }

            // 디렉토리 여부 확인
            if (!S_ISDIR(sb.st_mode)) 
            {
                continue;
            }

            stack.push_back(subdir);
        }

        closedir(dir);
    }

    return true;
}

std::string ScanWatchThread::getPathFromFd(int fd)
{
    char fdPath[64];
    snprintf(fdPath, sizeof(fdPath), "/proc/self/fd/%d", fd);

    char resolved[PATH_MAX];
    ssize_t len = readlink(fdPath, resolved, sizeof(resolved) - 1);
    if (len == -1) 
    {
        return "";
    }
    resolved[len] = '\0';
    return std::string(resolved);
}

void ScanWatchThread::processFanotifyEvents()
{
    // mFanFd로부터 fanotify 이벤트 데이터를 읽기
    char buffer[4096];
    ssize_t len = read(mFanFd, buffer, sizeof(buffer));
    if (len <= 0) 
    {
        return;
    }

    // 이벤트 반복 처리 -> 여러 개의 이벤트 존재 가능
    struct fanotify_event_metadata* metadata;
    for (metadata = (struct fanotify_event_metadata*)buffer;
         FAN_EVENT_OK(metadata, len);
         metadata = FAN_EVENT_NEXT(metadata, len)) 
         {

        // 실행 이벤트 처리
        if (metadata->mask & FAN_OPEN_EXEC_PERM) 
        {
            std::string path = getPathFromFd(metadata->fd);
            bool isMalicious = false;

            // 악성 검사
            if (!path.empty()) 
            {
                try 
                {
                    std::uintmax_t size = fs::file_size(path);
                    const std::uintmax_t SYSTEM_MAX = 100 * 1024 * 1024;
                    std::uintmax_t scanLimit = std::min(mMaxScanSizeBytes, SYSTEM_MAX);

                    if (size > scanLimit) 
                    {
                        spdlog::info("파일 크기 초과로 검사 제외 ({} bytes): {}", size, path);
                    }
                    else 
                    {
                        spdlog::info("실행 감지됨: {}", path);
                        isMalicious = ScanAndWait(path);
                    }
                } 
                catch (const std::exception& e) 
                {
                    spdlog::warn("파일 크기 확인 실패: {} - {}", path, e.what());
                }
            } 
            else 
            {
                spdlog::debug("비어있는 경로");
            }

            // 커널에게 실행 여부를 알림
            struct fanotify_response response = 
            {
                .fd = metadata->fd,
                .response = static_cast<__u32>(isMalicious ? FAN_DENY : FAN_ALLOW)
            };

            write(mFanFd, &response, sizeof(response));
            close(metadata->fd);
        }
    }
}

bool ScanWatchThread::ScanAndWait(const std::string& path)
{
    // prom : 값을 나중에 스캔 스레드가 전달
    std::promise<bool> prom;
    std::future<bool> fut = prom.get_future();

    // path와 결과 통신을 위한 promise를 함께 담은 ScanRequest
    ScanRequest req{ path, std::move(prom) };
    ScanQueue::GetInstance().Push(std::move(req)); 

    // 검사 결과 최대 3초 대기
    // 3초 안에 응답이 오면 → 아래에서 결과를 fut.get()으로 받아서 반환
    if (fut.wait_for(std::chrono::seconds(3)) == std::future_status::ready) 
    {
        return fut.get();  // true = 악성 → 차단, false = 정상 → 허용
    } 
    else 
    {
        spdlog::warn("검사 타임아웃: {}", path);
        return false;  // 타임아웃 시 기본 허용
    }
}