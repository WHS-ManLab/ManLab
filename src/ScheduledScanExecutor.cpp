#include "ScheduledScanExecutor.h"
#include "ScheduledScan.h"
#include <chrono>
#include <thread>

void ScheduledScanExecutor::Init(std::atomic<bool>& shouldRun,
                                 std::atomic<bool>& scanRequested,
                                 std::mutex& mutex,
                                 std::condition_variable& condVar)
{
    mpShouldRun = &shouldRun;
    mpScanRequested = &scanRequested;
    mpMutex = &mutex;
    mpCondVar = &condVar;
}

void ScheduledScanExecutor::Run()
{
    ScheduledScan scanner;

    while (*mpShouldRun)
    {
        //wait()가 뮤텍스를 자동으로 해제하고 다시 잡는 것을 지원
        std::unique_lock<std::mutex> lock(*mpMutex);
        
        //mpScanRequested == true 일 때 까지 대기
        mpCondVar->wait(lock, [&]() {
            return *mpScanRequested || !*mpShouldRun;
        }); // W의 mpCondVar->notify_one() 를 받고 깨어남

        if (!*mpShouldRun) 
        {
            break;
        }

        *mpScanRequested = false;
        lock.unlock();

        scanner.RunScan();
    }
}
