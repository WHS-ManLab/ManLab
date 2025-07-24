#include "ScheduledReportExecutor.h"
#include "ReportService.h"  // 리포트 작성 기능 담당 (별도 클래스 가정)
#include <chrono>
#include <iostream>
#include <spdlog/spdlog.h>

ScheduledReportExecutor::ScheduledReportExecutor()
{}

void ScheduledReportExecutor::Init(std::atomic<bool>& shouldRun,
                                   std::atomic<bool>& reportRequested,
                                   std::mutex& mutex,
                                   std::condition_variable& condVar)
{
    mpShouldRun = &shouldRun;
    mpReportRequested = &reportRequested;
    mpMutex = &mutex;
    mpCondVar = &condVar;
    spdlog::debug("ScheduledReportExecutor 초기화 완료.");
}

void ScheduledReportExecutor::Run()
{
    spdlog::info("ScheduledReportExecutor 스레드 시작됨.");
    ReportService service;
    while (*mpShouldRun)
    {
        std::unique_lock<std::mutex> lock(*mpMutex);
        spdlog::debug("리포트 요청 대기 중...");
        mpCondVar->wait(lock, [this] {
            return *mpReportRequested || !*mpShouldRun;
        });

        if (!*mpShouldRun)
        {
            spdlog::info("ScheduledReportExecutor 종료 플래그 감지됨. 스레드 종료.");
            break;
        }

        spdlog::info("예약 리포트 요청 감지됨. 실행 시작.");
        *mpReportRequested = false;  // 요청 처리 완료
        lock.unlock();
        service.Run();
        spdlog::info("ReportService::Run() 완료.");
    }
    spdlog::info("ScheduledReportExecutor 스레드 종료됨.");
}