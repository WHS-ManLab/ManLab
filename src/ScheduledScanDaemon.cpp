#include "ScheduledScanDaemon.h"  
#include "ScheduledScan.h"
#include <thread>
#include <chrono>
#include <iostream>

std::atomic<bool> ScheduledScanDaemon::sbRunning(true);

using namespace std;

void ScheduledScanDaemon::Run()
{
    daemonize();
    setupSignalHandlers();

    ScheduledScan scanner;

    while (sbRunning) {
        std::chrono::system_clock::time_point nextTrigger = scanner.GetNextTriggerTime();
        scanner.WaitUntil(nextTrigger);

        if (!sbRunning) break;

        scanner.RunScan();

        // 1분간 쉬기 — 중복 실행 방지
        // 너무 짧은 간격(1분 내외) 예약은 지원하지 않는다 알림
        std::this_thread::sleep_for(std::chrono::minutes(1));
    }
}