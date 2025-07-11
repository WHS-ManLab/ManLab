#pragma once

#include <atomic>
#include <mutex>
#include <condition_variable>

class ScheduledScanExecutor
{
public:
    ScheduledScanExecutor() = default;

    void Init(std::atomic<bool>& shouldRun,
              std::atomic<bool>& scanRequested,
              std::mutex& mutex,
              std::condition_variable& condVar);

    void Run();

private:
    std::atomic<bool>* mpShouldRun;
    std::atomic<bool>* mpScanRequested;
    std::mutex* mpMutex;
    std::condition_variable* mpCondVar;
};
