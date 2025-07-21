#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <string>

class ScanQueue 
{
public:
    static ScanQueue& GetInstance() 
    {
        static ScanQueue instance;
        return instance;
    }

    void Push(const std::string& path);
    bool Pop(std::string& outPath);
    void Stop();

private:
    ScanQueue() = default;
    ScanQueue(const ScanQueue&) = delete;
    ScanQueue& operator=(const ScanQueue&) = delete;

    std::queue<std::string> mQueue;
    std::mutex mMutex;
    std::condition_variable mCond;
    bool mbStop = false;
};