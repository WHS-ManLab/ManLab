#pragma once
#include <atomic>

class RealtimeMonitorDaemon 
{
public:
    void Init(std::atomic<bool>& shouldRun);
    void Run();

private:
    std::atomic<bool>* mpShouldRun = nullptr;
};