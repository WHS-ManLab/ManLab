#include "RealtimeMonitorDaemon.h"
#include "RealTimeMonitor.h"
#include <thread>
#include <chrono>
#include <vector>
#include <syslog.h>

std::atomic<bool> RealtimeMonitorDaemon::sbRunning(true);



void RealtimeMonitorDaemon::Run()
{
    //daemonize();            // 백그라운드 데몬화
    //setupSignalHandlers();  // SIGTERM 등 처리


    // 1. INI 파일에서 경로, 마스크 파싱
    auto pathMaskList = parsePathsFromIni("/home/rlacofla/Manlab/ManLab/conf/FIMConfig.ini");

    // 2. 경로 목록 생성
    std::vector<std::string> watchDirs;
    for (const auto& [path, _] : pathMaskList)
    {
        watchDirs.push_back(path);

    }

    // 3. 감시 객체 생성
    RealTimeMonitor watcher(watchDirs);
    for (const auto& [path, mask] : pathMaskList)
    {
        watcher.AddWatchWithFilter(path, mask);
    }


    // 3. 초기화           
    watcher.Init();
 
    watcher.Start();


    // 4. 감지 루프
    while (sbRunning) 
    {
        watcher.pollOnce();  //

        std::this_thread::sleep_for(std::chrono::milliseconds(100)); 
    }
}