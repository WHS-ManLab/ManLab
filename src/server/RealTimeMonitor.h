#pragma once

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


// 사용자 친화적 이벤트 마스크 정의
enum CustomEvent : uint64_t {
    CREATE  = 0x01,
    DELETE  = 0x02,
    MODIFY  = 0x04,
    ATTRIB  = 0x08
};


constexpr size_t BUF_SIZE = 4096;
constexpr size_t FD_PATH_SIZE = 128;
std::vector<std::pair<std::string, uint64_t>> parsePathsFromIni(const std::string& iniPath);
uint64_t parseCustomEventMask(const std::string& eventsStr);
uint64_t mapActualMaskToCustomMask(uint64_t actualMask);


class RealTimeMonitor {
public:
    RealTimeMonitor(const std::vector<std::string>& watchDirs);
    ~RealTimeMonitor();


    void AddWatchWithFilter(const std::string& path, uint64_t eventMask);
    bool ShouldDisplayEvent(const std::string& path, uint64_t mask);
    bool Init();
    void Start();
    void pollOnce();

private:
    int mFanFd;
    int mInotifyFd;
    std::array<char, BUF_SIZE> mBuf;
    std::array<char, FD_PATH_SIZE> mFdPath;
    std::array<char, PATH_MAX + 1> mPath;
    std::vector<std::string> mMountPoints; 
    std::vector<int> mMountFds; 
    std::vector<std::string> mWatchDirs;

    void printErrorAndExit(const std::string& msg);
    void processFanotifyEvents(struct fanotify_event_metadata* metadata, ssize_t bufLen); 
    int findMountFdForFileHandle(const struct file_handle* fid); 
    std::unordered_map<std::string, uint64_t> mUserEventFilters;
    std::unordered_map<int, std::string> mInotifyWdToPath;
    void processInotifyEvents();


};
