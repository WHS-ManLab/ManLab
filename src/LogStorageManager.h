#pragma once
#include "DBManager.h"

class LogStorageManager
{
public:
    LogStorageManager();
    void Run(LogAnalysisResult& result, bool bIsAudit);

private:
    StorageLogAnalysisResult* mStorage = nullptr;
    static std::string convertTimestamp(const std::string &rawTimestamp, bool bIsAudit);
};