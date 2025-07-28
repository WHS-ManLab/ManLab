#pragma once

#include <atomic>
#include <mutex>
#include <condition_variable>

class ScheduleWatcher
{
public:
    ScheduleWatcher() = default;

    void Init(std::atomic<bool>& shouldRun,
            std::atomic<bool>& scanRequested,
            std::atomic<bool>& reportRequested,
            std::mutex& scanMutex,
            std::condition_variable& scanCondVar,
            std::mutex& reportMutex,
            std::condition_variable& reportCondVar);

    void Run();

private:
    std::atomic<bool>* mpShouldRun = nullptr;
    std::atomic<bool>* mpScanRequested = nullptr;
    std::atomic<bool>* mpReportRequested = nullptr;

    std::mutex* mpScanMutex = nullptr;
    std::condition_variable* mpScanCondVar = nullptr;

    std::mutex* mpReportMutex = nullptr;
    std::condition_variable* mpReportCondVar = nullptr;
};