#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <unordered_map>

class ScanWatchThread
{
public:
    ScanWatchThread();
    ~ScanWatchThread();

    void Init(std::atomic<bool>& shouldRun);
    void Run();

private:
    void loadWatchConfig();
    bool shouldScan(const std::string& path);
    void watchLoop();
    void processInotifyEvents();
    void addWatchDir(const std::string& path);
    void addWatchDirRecursive(const std::string& path);

    std::atomic<bool>* mpShouldRun;
    int mInotifyFd;
    std::unordered_map<int, std::string> mWdToPath;
    std::vector<std::string> mWatchDirs;
    std::vector<std::string> mExcludeDirs;
};
