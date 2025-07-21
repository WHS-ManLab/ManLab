#include <spdlog/spdlog.h>

#include "ServerDaemon.h"
#include "DBManager.h"
#include "DaemonUtils.h"
#include "Paths.h"

ServerDaemon::ServerDaemon()
    : mShouldRun(true),
      mbScanRequested(false),
      mbReportRequested(false)
{}

void ServerDaemon::Run()
{
    // DB 초기화
    spdlog::info("데이터베이스 스키마 초기화");
    DBManager::GetInstance().InitSchema();
    spdlog::info("악성코드 해시 DB 초기화");
    DBManager::GetInstance().InitHashDB();

    // 각 데몬 Init
    spdlog::info("데몬 초기화 시작");
    mScheduleWatcher.Init(mShouldRun, mbScanRequested, mbReportRequested, mScheduleMutex, mScheduleCondVar, mReportMutex, mReportCondVar);
    mScheduledScanExecutor.Init(mShouldRun, mbScanRequested, mScheduleMutex, mScheduleCondVar);
    mRealTimeScanWorker.Init(mShouldRun); 
    mScheduledReportExecutor.Init(mShouldRun, mbReportRequested, mReportMutex, mReportCondVar);
    mRealtimeMonitorDaemon.Init(mShouldRun);
    mRsyslogManager.Init(mShouldRun);
    mCommandReceiver.Init(*this, mShouldRun);
    mScanWatchThread.Init(mShouldRun);
    mAuditLogManager.Init(mShouldRun);
    spdlog::info("모든 데몬 초기화 완료");

    // 다른 인스턴스가 실행 중이 아니면 실행
    spdlog::info("다른 인스턴스가 실행 중인지 확인 후 실행 시도");
    LaunchDaemonIfNotRunning("ManLabCommandDaemon", [this]() { this->startWorkerThreads(); });
}

void ServerDaemon::startWorkerThreads()
{
    spdlog::info("startWorkerThreads(): 워커 스레드 실행 시작");
    mThreads.emplace_back(&RealtimeMonitorDaemon::Run, &mRealtimeMonitorDaemon);
    mThreads.emplace_back(&ScheduleWatcher::Run, &mScheduleWatcher);
    mThreads.emplace_back(&ScheduledScanExecutor::Run, &mScheduledScanExecutor);
    mThreads.emplace_back(&RealTimeScanWorker::Run, &mRealTimeScanWorker);
    mThreads.emplace_back(&RsyslogManager::Run, &mRsyslogManager);
    mThreads.emplace_back(&CommandReceiver::Run, &mCommandReceiver);
    mThreads.emplace_back(&ScheduledReportExecutor::Run, &mScheduledReportExecutor);
    mThreads.emplace_back(&ScanWatchThread::Run, &mScanWatchThread);
    mThreads.emplace_back(&AuditLogManager::Run, &mAuditLogManager);
    
    joinWorkerThreads();
    spdlog::info("startWorkerThreads(): 모든 스레드 종료됨");
    cleanup();
    spdlog::info("startWorkerThreads(): cleanup() 호출 완료");
}

// 모든 스레드 종료시까지 대기
void ServerDaemon::joinWorkerThreads()
{
    for (auto& t : mThreads)
    {
        if (t.joinable())
        {
            t.join();
        }
    }
}

void ServerDaemon::Stop()
{
    mShouldRun = false;

    // 예약 검사 조건 변수 해제
    mScheduleCondVar.notify_all();
}

void ServerDaemon::cleanup()
{
    StopDaemon("ManLabCommandDaemon");
}