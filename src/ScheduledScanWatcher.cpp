#include "ScheduledScanWatcher.h"
#include "ScheduledScan.h"
#include <chrono>
#include <thread>

void ScheduledScanWatcher::Init(std::atomic<bool>& shouldRun,
                                std::atomic<bool>& scanRequested,
                                std::mutex& mutex,
                                std::condition_variable& condVar)
{
    mpShouldRun = &shouldRun;
    mpScanRequested = &scanRequested;
    mpMutex = &mutex;
    mpCondVar = &condVar;
}

void ScheduledScanWatcher::Run()
{
    ScheduledScan scanner;

    while (*mpShouldRun)
    {
        // 스캔 요청
        if (scanner.ShouldTriggerNow())
        {
            { 
                std::lock_guard<std::mutex> lock(*mpMutex); 
                *mpScanRequested = true;    // 스캔 요청 플래그
            }
            // Execyutor wait 해제
            mpCondVar->notify_one();
            std::this_thread::sleep_for(std::chrono::seconds(60));
        }
        if (!*mpShouldRun) 
        {
            break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(30));
    }
}
