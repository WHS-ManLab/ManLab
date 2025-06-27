#include "LogDaemon.h"
#include <thread>
#include <chrono>

void LogCollectorDaemon::run()
{
    daemonize();
    handleSignals();

    while (running) {
        // TODO: 로그 파일 감시 및 분석
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}