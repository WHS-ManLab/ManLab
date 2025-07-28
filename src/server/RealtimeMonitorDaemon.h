#pragma once
#include <atomic>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>


class RealtimeMonitorDaemon 
{
public:
    void Init(std::atomic<bool>& shouldRun);
    void Run();

private:
    std::atomic<bool>* mpShouldRun = nullptr;
};