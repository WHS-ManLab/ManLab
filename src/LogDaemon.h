#pragma once

#include "DaemonBase.h"
#include <csignal>
#include <atomic>

class LogCollectorDaemon : public DaemonBase {
public:
    void Run() override;

    // running 상태 반환 함수
    static bool IsRunning()
    {
        return sbRunning;
    }

protected:
    // SIGTERM, SIGINT 핸들러 등록
    void setupSignalHandlers() override
    {
        signal(SIGTERM, [](int) { sbRunning = false; });
        signal(SIGINT,  [](int) { sbRunning = false; });
    }

private:
    static std::atomic<bool> sbRunning;
};