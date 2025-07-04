#include "ScheduledScanDaemon.h"  
#include "ScheduledScan.h"
#include <thread>
#include <chrono>
#include <iostream>

std::atomic<bool> ScheduledScanDaemon::running(true);

using namespace std;

void ScheduledScanDaemon::run()
{
    daemonize();
    setupSignalHandlers();

    ScheduledScan scanner;

    while (running) {
        // 예약된 다음 시간 확인
        std::chrono::system_clock::time_point nextTrigger = scanner.getNextTriggerTime();

        // 다음 실행 시각까지 대기
        scanner.waitUntil(nextTrigger);

        if (!running) break;

        // 스캔 수행
        scanner.runScan();  
    }
}