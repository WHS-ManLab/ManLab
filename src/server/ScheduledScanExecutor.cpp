#include "ScheduledScanExecutor.h"
#include "ScheduledScan.h"
#include <chrono>
#include <thread>
#include <spdlog/spdlog.h>

void ScheduledScanExecutor::Init(std::atomic<bool>& shouldRun,
                                 std::atomic<bool>& scanRequested,
                                 std::mutex& mutex,
                                 std::condition_variable& condVar)
{
    mpShouldRun = &shouldRun;
    mpScanRequested = &scanRequested;
    mpMutex = &mutex;
    mpCondVar = &condVar;
    spdlog::debug("ScheduledScanExecutor 초기화 완료.");
}

void ScheduledScanExecutor::Run()
{
    spdlog::info("ScheduledScanExecutor 스레드 시작됨.");
    ScheduledScan scanner;

    while (*mpShouldRun)
    {
        //wait()가 뮤텍스를 자동으로 해제하고 다시 잡는 것을 지원
        std::unique_lock<std::mutex> lock(*mpMutex);
        spdlog::debug("검사 요청 대기 중...");
        
        //mpScanRequested == true 일 때 까지 대기
        mpCondVar->wait(lock, [&]() {
            return *mpScanRequested || !*mpShouldRun;
        }); // W의 mpCondVar->notify_one() 를 받고 깨어남

        if (!*mpShouldRun) 
        {
            spdlog::info("종료 플래그 감지됨. ScheduledScanExecutor 스레드 종료.");
            break;
        }

        spdlog::info("예약 검사 요청 감지됨. 실행 시작.");
        *mpScanRequested = false;
        lock.unlock();

        scanner.RunScan();
        spdlog::info("ScheduledScan::RunScan() 완료.");
    }
    spdlog::info("ScheduledScanExecutor 스레드 완전히 종료됨.");
}
