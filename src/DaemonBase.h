#pragma once

class DaemonBase {
public:
    virtual void run() = 0;
    virtual ~DaemonBase() = default;

protected:
    void daemonize();             // 세션 전환
    static void handleSignals();  // SIGTERM 등 처리
    static volatile bool running; // while(running)에서 사용. SIGTERM등 신호 시 종료
};