#include "ScanQueue.h"

void ScanQueue::Push(ScanRequest&& req)
{
    std::lock_guard<std::mutex> lock(mMutex);
    mQueue.push(std::move(req));
    mCond.notify_one();
}

bool ScanQueue::Pop(ScanRequest& outReq)
{
    std::unique_lock<std::mutex> lock(mMutex);
    mCond.wait(lock, [&]() {
        return !mQueue.empty() || mbStop;
    });

    if (mbStop && mQueue.empty()) 
    {
        return false;
    }

    outReq = std::move(mQueue.front());
    mQueue.pop();
    return true;
}

void ScanQueue::Stop()
{
    std::lock_guard<std::mutex> lock(mMutex);
    mbStop = true;
    mCond.notify_all();
}