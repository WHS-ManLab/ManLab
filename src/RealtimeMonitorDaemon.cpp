#include "RealtimeMonitorDaemon.h"
#include <thread>
#include <chrono>

void RealtimeMonitorDaemon::run()
{
    daemonize();
    handleSignals();

    while (running) {
        // TODO: inotify 또는 fanotify 이벤트 감지
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}