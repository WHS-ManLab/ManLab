#include "RealtimeMonitorDaemon.h"
#include "RealTimeMonitor.h"
#include "FimLogToDB.h"
#include "Paths.h"

#include <thread>
#include <chrono>
#include <vector>
#include <syslog.h>


void RealtimeMonitorDaemon::Init(std::atomic<bool>& shouldRun)
{
    mpShouldRun = &shouldRun;
}


void RealtimeMonitorDaemon::Run()
{   


    try {
    auto logger = spdlog::rotating_logger_mt(
    "file_logger",
    "/root/ManLab/log/RealTimeMonitor.log",
    1024 * 1024 * 5, // 최대 파일 크기 5MB
    3                // 최대 파일 개수 3개
    );
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::info);
    spdlog::flush_on(spdlog::level::info);

    spdlog::info("Logging started");
    spdlog::default_logger()->flush();
    } 
    catch (const spdlog::spdlog_ex &ex) 
    {
        std::cerr << "Log init failed: " << ex.what() << std::endl;
    }

    // 1. INI 파일에서 경로, 마스크 파싱
    auto pathMaskList = parsePathsFromIni(PATH_FIM_CONFIG_INI, std::cerr);

    // 2. 경로 목록 생성
    std::vector<std::string> watchDirs;
    for (const auto& [path, _] : pathMaskList)
    {
        watchDirs.push_back(path);

    }

    // 3. 감시 객체 생성
    RealTimeMonitor watcher(watchDirs);
    watcher.parseExcludeFromIni(PATH_FIM_CONFIG_INI);
    
    for (const auto& [path, mask] : pathMaskList)
    {
        watcher.AddWatchWithFilter(path, mask);
    }


    // 3. 초기화           
    watcher.Init();
 

    auto lastDbUpdate = std::chrono::steady_clock::now();

    FimLogToDB parser;  // 반복문 밖에서 한 번만 만들기

    std::string lastSavedTime = "1970-01-01 00:00:00";

    while (*mpShouldRun) {
        watcher.pollOnce();

        auto now = std::chrono::steady_clock::now();
        if (now - lastDbUpdate >= std::chrono::minutes(1)) {
            parser.ParseAndStore("/root/ManLab/log/RealTimeMonitor.log", lastSavedTime);
            lastDbUpdate = now;

            // 최신 저장 시간 갱신 (ParseAndStore에서 최신 시간 받아와야 함)
            lastSavedTime = parser.GetLatestTimestamp();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

