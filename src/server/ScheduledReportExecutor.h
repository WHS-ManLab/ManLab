#pragma once

#include <atomic>
#include <mutex>
#include <condition_variable>

class ScheduledReportExecutor
{
public:
    ScheduledReportExecutor();

    void Init(std::atomic<bool>& shouldRun,
              std::atomic<bool>& reportRequested,
              std::mutex& mutex,
              std::condition_variable& condVar);

    void Run();  // 스레드 메인 루프
    
private:
    std::atomic<bool>* mpShouldRun = nullptr;
    std::atomic<bool>* mpReportRequested = nullptr;
    std::mutex* mpMutex = nullptr;
    std::condition_variable* mpCondVar = nullptr;
};