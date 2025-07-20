#pragma once

#include "Paths.h"
#include <algorithm>
#include <iostream>
#include <cstring>
#include <sys/fanotify.h>
#include <sys/types.h>
#include <limits.h>
#include <linux/fanotify.h>
#include <fcntl.h>
#include <array>
#include <string>
#include <vector>
#include <unordered_map>
#include <poll.h>
#include <unistd.h>
#include <set>
#include <sstream>
#include <chrono>
#include <unordered_set>
#include <sys/stat.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

// 사용자 친화적 이벤트 마스크 정의
enum CustomEvent : uint64_t {
    CREATE  = 0x01,
    DELETE  = 0x02,
    MODIFY  = 0x04,
    ATTRIB  = 0x08,
    RENAME  = 0x10 //리네임추가
};

extern std::shared_ptr<spdlog::logger> RealTime_logger;
constexpr size_t BUF_SIZE = 4096;
std::vector<std::pair<std::string, uint64_t>> parsePathsFromIni(const std::string& iniPath, std::ostream& err);
uint64_t parseCustomEventMask(const std::string& eventsStr, std::ostream& err);
uint64_t mapActualMaskToCustomMask(uint64_t actualMask);
inline std::unordered_map<std::string, std::string> realToUserPathMap;

class RealTimeMonitor {
public:
    RealTimeMonitor(const std::vector<std::string>& watchDirs);
    ~RealTimeMonitor();

    void parseExcludeFromIni(const std::string& iniFilePath);
    void AddWatchWithFilter(const std::string& path, uint64_t eventMask);
    bool ShouldDisplayEvent(const std::string& path, uint64_t mask);
    bool Init();
    void pollOnce();

private:
    int mFanFd;
    int mInotifyFd;
    std::array<char, BUF_SIZE> mBuf;
    std::array<char, PATH_MAX + 1> mPath;
    std::vector<int> mMountFds; 
    std::vector<std::string> mWatchDirs;
    std::unordered_map<std::string, std::pair<uint64_t, std::chrono::steady_clock::time_point>> mRecentEvents;
    std::unordered_map<std::string, std::unordered_set<std::string>> excludeFilesByPath;
    std::unordered_map<uint64_t, std::chrono::steady_clock::time_point> mRecentModifiedInodes;

    void registerWatchPath(const std::string& userPath);
    std::string mapToUserPath(const std::string& realPath) const;
    bool IsExcludedFile(const std::string& fullPath);
    uint64_t getInodeFromFd(int fd);
    bool isDuplicateEvent(uint64_t inode);
    void printErrorAndExit(const std::string& msg, std::ostream& err);
    void processFanotifyEvents(struct fanotify_event_metadata* metadata, ssize_t bufLen, std::ostream& err); 
    int findMountFdForFileHandle(const struct file_handle* fid); 
    std::unordered_map<std::string, uint64_t> mUserEventFilters;
    std::unordered_map<int, std::string> mInotifyWdToPath;
    void processInotifyEvents();


};
