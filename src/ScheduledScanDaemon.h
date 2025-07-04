#pragma once

#include "DaemonBase.h"
#include <atomic>

class ScheduledScanDaemon : public DaemonBase {
public:
    void run() override;

protected:
    void setupSignalHandlers() override {
        signal(SIGTERM, [](int){ running = false; });
        signal(SIGINT,  [](int){ running = false; });
    }

private:
    static std::atomic<bool> running;
};