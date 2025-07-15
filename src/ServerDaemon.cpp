#include "ServerDaemon.h"
#include "DBManager.h"
#include "DaemonUtils.h"
#include "Paths.h"

ServerDaemon::ServerDaemon()
    : mShouldRun(true),
      mbScanRequested(false)
{}

void ServerDaemon::Run(bool systemdMode)
{
    // DB 초기화
    DBManager::GetInstance().InitSchema();
    DBManager::GetInstance().InitHashDB();

    // 각 데몬 Init
    mScheduledScanWatcher.Init(mShouldRun, mbScanRequested, mScheduleMutex, mScheduleCondVar);
    mScheduledScanExecutor.Init(mShouldRun, mbScanRequested, mScheduleMutex, mScheduleCondVar);
    mRealtimeMonitorDaemon.Init(mShouldRun);
    mRsyslogManager.Init(mShouldRun);
    mCommandReceiver.Init(*this, mShouldRun);

    // 다른 인스턴스가 실행 중이 아니면 실행
    LaunchDaemonIfNotRunning(systemdMode, "ManLabCommandDaemon", [this]() { this->startWorkerThreads(); });
}

void ServerDaemon::startWorkerThreads()
{
    mThreads.emplace_back(&RealtimeMonitorDaemon::Run, &mRealtimeMonitorDaemon);
    mThreads.emplace_back(&ScheduledScanWatcher::Run, &mScheduledScanWatcher);
    mThreads.emplace_back(&ScheduledScanExecutor::Run, &mScheduledScanExecutor);
    mThreads.emplace_back(&RsyslogManager::Run, &mRsyslogManager);
    mThreads.emplace_back(&CommandReceiver::Run, &mCommandReceiver);

    joinWorkerThreads();
    cleanup();
}

// 모든 스레드 종료시까지 대기
void ServerDaemon::joinWorkerThreads()
{
    for (auto& t : mThreads)
    {
        if (t.joinable())
            t.join();
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