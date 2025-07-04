#pragma once
#include "DaemonBase.h"
#include <csignal>
#include <atomic>

class LogCollectorDaemon : public DaemonBase {
public:
    void run() override;

    // running 상태 반환 함수
    static bool isRunning() {
        return running;
    }

protected:
    // SIGTERM, SIGINT 핸들러 등록
    void setupSignalHandlers() override {
        signal(SIGTERM, [](int) { running = false; });
        signal(SIGINT,  [](int) { running = false; });
    }

private:
    static std::atomic<bool> running;
};