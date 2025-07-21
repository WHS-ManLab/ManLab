#pragma once

#include <atomic>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "RsyslogManager.h"
#include "RealtimeMonitorDaemon.h"
#include "ScheduleWatcher.h"
#include "ScheduledScanExecutor.h"
#include "CommandReceiver.h"
#include "ScheduledReportExecutor.h"
#include "RealTimeScanWorker.h" 
#include "ScanWatchThread.h" 
#include "AuditLogManager.h"

class ServerDaemon
{
public:
    ServerDaemon();
    void Run();  // 데몬 실행 시작
    void Stop();                 // 데몬 종료 요청

private:
    void startWorkerThreads();    // 데몬 스레드 시작
    void joinWorkerThreads();     // 스레드 종료 대기
    void cleanup();               // 리소스 정리

    std::atomic<bool> mShouldRun;

    // 데몬 구성 요소
    RsyslogManager mRsyslogManager;
    RealtimeMonitorDaemon mRealtimeMonitorDaemon;
    RealTimeScanWorker mRealTimeScanWorker;   
    ScheduleWatcher mScheduleWatcher;
    ScheduledScanExecutor mScheduledScanExecutor;
    CommandReceiver mCommandReceiver;
    AuditLogManager mAuditLogManager;
    ScheduledReportExecutor mScheduledReportExecutor;
    ScanWatchThread mScanWatchThread;

    // 실행 중인 스레드들
    std::vector<std::thread> mThreads;

    // 예약 검사 공유 상태
    std::atomic<bool> mbScanRequested;
    std::mutex mScheduleMutex;
    std::condition_variable mScheduleCondVar;

    // 리포트 공유 상태
    std::atomic<bool> mbReportRequested;
    std::mutex mReportMutex;
    std::condition_variable mReportCondVar;
};