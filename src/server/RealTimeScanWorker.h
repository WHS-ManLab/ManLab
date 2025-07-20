#pragma once
#include <atomic>
#include <string>

class RealTimeScanWorker 
{
public:
    void Init(std::atomic<bool>& shouldRun);
    void Run();  

private:
    std::atomic<bool>* mpShouldRun = nullptr;
};