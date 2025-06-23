#include "ScheduledScanDaemon.h"
#include <thread>
#include <chrono>

void ScheduledScanDaemon::run()
{
    daemonize();
    handleSignals();

    while (running) {
        // TODO: 시간 체크 → 예약 시간 도달 시 ScanMalware 실행
        std::this_thread::sleep_for(std::chrono::seconds(60));
    }
}