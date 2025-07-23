#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <string>
#include <future>

struct ScanRequest
{
    std::string path;
    std::promise<bool> resultPromise;
};

class ScanQueue 
{
public:
    static ScanQueue& GetInstance() 
    {
        static ScanQueue instance;
        return instance;
    }

    void Push(ScanRequest&& req);      
    bool Pop(ScanRequest& outReq);      
    void Stop();

private:
    ScanQueue() = default;
    ScanQueue(const ScanQueue&) = delete;
    ScanQueue& operator=(const ScanQueue&) = delete;

    std::queue<ScanRequest> mQueue;    
    std::mutex mMutex;
    std::condition_variable mCond;
    bool mbStop = false;
};