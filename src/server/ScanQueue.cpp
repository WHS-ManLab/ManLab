#include "ScanQueue.h"

void ScanQueue::Push(const std::string& path) 
{
    std::lock_guard<std::mutex> lock(mMutex);
    mQueue.push(path);
    mCond.notify_one();
}

bool ScanQueue::Pop(std::string& outPath) 
{
    std::unique_lock<std::mutex> lock(mMutex);
    mCond.wait(lock, [&]() { return !mQueue.empty() || mbStop; });

    if (mbStop && mQueue.empty()) return false;
    outPath = mQueue.front();
    mQueue.pop();
    return true;
}

void ScanQueue::Stop() 
{
    std::lock_guard<std::mutex> lock(mMutex);
    mbStop = true;
    mCond.notify_all();
}