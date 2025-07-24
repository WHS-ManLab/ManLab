#include "RealTimeScanWorker.h"
#include "ScanQueue.h"
#include "MalwareScan.h"
#include <spdlog/spdlog.h>

void RealTimeScanWorker::Init(std::atomic<bool>& shouldRun)
{
    mpShouldRun = &shouldRun;
}

void RealTimeScanWorker::Run()
{
    spdlog::info("RealTimeScanWorker 시작됨");

    MalwareScan scanner;
    try 
    {
        scanner.Init();  // YARA 및 해시 초기화
    } 
    catch (const std::exception& e) 
    {
        spdlog::error("MalwareScan 초기화 실패: {}", e.what());
        return;
    }

    auto& queue = ScanQueue::GetInstance();

    while (*mpShouldRun)
    {
        //ScanQueue에서 검사 요청을 가져옴
        ScanRequest req;
        if (!queue.Pop(req)) 
        {
            break;
        }

        bool isMalicious = false;
        try 
        {
            // 경로에 대해 악성 검사 수행
            isMalicious = scanner.RunSingleFile(req.path, true);  // 격리 포함
        } 
        catch (const std::exception& e) 
        {
            spdlog::warn("실시간 검사 실패: {}, 이유: {}", req.path, e.what());
        }

        try 
        {
            // promise 객체에 검사 결과 전달
            // ScanAndWait()에서 future로 받음
            req.resultPromise.set_value(isMalicious);
        } 
        catch (...) 
        {
            spdlog::debug("promise 결과 설정 실패 (이미 응답됨?): {}", req.path);
        }
    }

    spdlog::info("RealTimeScanWorker 종료됨");
}