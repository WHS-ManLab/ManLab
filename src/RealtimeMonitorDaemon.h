#pragma once
#include "DaemonBase.h"
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>

class RealtimeMonitorDaemon : public DaemonBase {
public:
    void Run() override;

protected:
    void setupSignalHandlers() override {
        signal(SIGTERM, [](int){ sbRunning = false; });
        signal(SIGINT,  [](int){ sbRunning = false; });
    }

    static bool isRunning() 
    { 
        return sbRunning; 
    }

private:
    static std::atomic<bool> sbRunning;
};