#pragma once

class DaemonBase {
public:
    virtual void Run() = 0;
    virtual ~DaemonBase() = default;

protected:
    // 세션 전환, 표준 입출력 차단
    void daemonize();

    // 시그널 핸들러 등록 
    virtual void setupSignalHandlers() = 0;
};