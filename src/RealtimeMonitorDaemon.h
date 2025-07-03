#pragma once
#include "DaemonBase.h"
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>

class RealtimeMonitorDaemon : public DaemonBase {
public:
    void run() override;

protected:
    void setupSignalHandlers() override {
        signal(SIGTERM, [](int){ running = false; });
        signal(SIGINT,  [](int){ running = false; });
    }

    static bool isRunning() { return running; }

private:
    static std::atomic<bool> running;
};