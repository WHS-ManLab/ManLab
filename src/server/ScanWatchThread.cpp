#include "ScanWatchThread.h"
#include "INIReader.h"
#include "ScanQueue.h"

#include <unistd.h>
#include <dirent.h>
#include <sys/inotify.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <string.h>
#include <poll.h>
#include <errno.h>

ScanWatchThread::ScanWatchThread()
    : mpShouldRun(nullptr), 
      mInotifyFd(-1)
{}

ScanWatchThread::~ScanWatchThread()
{
    if (mInotifyFd >= 0) close(mInotifyFd);
}

void ScanWatchThread::Init(std::atomic<bool>& shouldRun)
{
    mpShouldRun = &shouldRun;
    loadWatchConfig();
}

void ScanWatchThread::Run()
{
    spdlog::info("ScanWatchThread: 감시 스레드 시작");

    mInotifyFd = inotify_init1(IN_NONBLOCK);
    if (mInotifyFd < 0) {
        spdlog::error("inotify 초기화 실패: {}", strerror(errno));
        return;
    }

    for (const auto& dir : mWatchDirs) {
        addWatchDirRecursive(dir);
    }

    watchLoop();
    spdlog::info("ScanWatchThread: 감시 스레드 종료");
}

void ScanWatchThread::loadWatchConfig()
{
    INIReader reader("/root/ManLab/conf/MalScanConfig.ini");

    std::string scanPaths = reader.Get("Scan", "paths", "");
    std::stringstream ss(scanPaths);
    std::string token;
    while (std::getline(ss, token, ',')) {
        token.erase(0, token.find_first_not_of(" \t"));
        token.erase(token.find_last_not_of(" \t") + 1);
        if (!token.empty()) mWatchDirs.push_back(token);
    }

    std::string excludePaths = reader.Get("Exclude", "paths", "");
    std::stringstream ss2(excludePaths);
    while (std::getline(ss2, token, ',')) {
        token.erase(0, token.find_first_not_of(" \t"));
        token.erase(token.find_last_not_of(" \t") + 1);
        if (!token.empty()) mExcludeDirs.push_back(token);
    }

    const std::vector<std::string> systemExcludes = {
        "/proc", "/sys", "/dev", "/run", "/tmp",
        "/var/tmp", "/var/run", "/var/cache"
    };

    mExcludeDirs.insert(mExcludeDirs.end(), systemExcludes.begin(), systemExcludes.end());
}

bool ScanWatchThread::shouldScan(const std::string& path)
{
    for (const auto& ex : mExcludeDirs) {
        if (path.find(ex) == 0) return false;
    }
    return true;
}

void ScanWatchThread::addWatchDir(const std::string& path)
{
    int wd = inotify_add_watch(mInotifyFd, path.c_str(), IN_CREATE);
    if (wd == -1) {
        spdlog::warn("inotify 감시 추가 실패: {} - {}", path, strerror(errno));
        return;
    }
    mWdToPath[wd] = path;
    spdlog::info("감시 등록: {}", path);
}

void ScanWatchThread::addWatchDirRecursive(const std::string& path)
{
    if (!shouldScan(path)) return;

    addWatchDir(path);

    DIR* dir = opendir(path.c_str());
    if (!dir) {
        spdlog::warn("디렉토리 열기 실패: {} - {}", path, strerror(errno));
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type != DT_DIR) continue;

        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;

        std::string subdir = path + "/" + name;
        addWatchDirRecursive(subdir);
    }

    closedir(dir);
}

void ScanWatchThread::watchLoop()
{
    struct pollfd fds[1];
    fds[0].fd = mInotifyFd;
    fds[0].events = POLLIN;

    while (mpShouldRun && *mpShouldRun) {
        int ret = poll(fds, 1, 1000); // 1초마다 poll
        if (ret <= 0) continue;

        if (fds[0].revents & POLLIN) {
            processInotifyEvents();
        }
    }
}

void ScanWatchThread::processInotifyEvents()
{
    char buffer[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
    ssize_t length = read(mInotifyFd, buffer, sizeof(buffer));
    if (length <= 0) return;

    for (char* ptr = buffer; ptr < buffer + length; ) {
        struct inotify_event* event = (struct inotify_event*)ptr;
        if (event->mask & IN_CREATE) {
            std::string dir = mWdToPath[event->wd];
            std::string fullPath = dir + "/" + event->name;

            if (shouldScan(fullPath)) {
                spdlog::info("파일 생성 감지: {}", fullPath);
                ScanQueue::GetInstance().Push(fullPath);
            } else {
                spdlog::debug("제외된 경로 무시: {}", fullPath);
            }
        }

        ptr += sizeof(struct inotify_event) + event->len;
    }
}
