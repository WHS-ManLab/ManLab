#pragma once

#include <atomic>
#include <string>
#include <vector>
#include <map>

class ScanWatchThread
{
public:
    ScanWatchThread();
    ~ScanWatchThread();

    void Init(std::atomic<bool>& shouldRun);
    void Run();

private:
    // 초기화 함수
    bool InitFanotify();
    void loadWatchConfig();

    // fanotify 이벤트 루프
    void processFanotifyEvents();

    // 경로 추출
    std::string getPathFromFd(int fd);

    // 대기
    bool ScanAndWait(const std::string& path);

private:
    std::atomic<bool>* mpShouldRun;
    int mFanFd;
    std::uintmax_t mMaxScanSizeBytes = 100 * 1024 * 1024;

    std::vector<std::string> mWatchDirs;
    std::vector<std::string> mExcludeDirs;
};