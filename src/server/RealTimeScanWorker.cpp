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
        std::string path;
        if (!queue.Pop(path)) break;

        try 
        {
            scanner.RunSingleFile(path, true);  // 격리 포함
        } 
        catch (const std::exception& e) 
        {
            spdlog::warn("실시간 검사 실패: {}, 이유: {}", path, e.what());
        }
    }

    spdlog::info("RealTimeScanWorker 종료됨");
}