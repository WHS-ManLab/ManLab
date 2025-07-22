#include "LogStorageManager.h"
#include <spdlog/spdlog.h> 

using namespace std;

// 생성자
LogStorageManager::LogStorageManager()
    : mStorage(&DBManager::GetInstance().GetLogAnalysisResultStorage())
{
}

// 로그 저장
void LogStorageManager::Run(LogAnalysisResult& result, bool bIsAudit)
{
    result.timestamp = convertTimestamp(result.timestamp, bIsAudit);
    spdlog::debug("result.type : {}", result.type);
    spdlog::debug("result.description : {}", result.description);
    spdlog::debug("result.timestamp : {}", result.timestamp);
    spdlog::debug("result.username : {}", result.username);
    spdlog::debug("result.originalLogPath : {}", result.originalLogPath);
    spdlog::debug("result.rawLine : {}", result.rawLine);
    mStorage->insert(result);
    spdlog::info("Saving to LogAnalysisResultDB has been completed.");
}

// 타임스탬프 형식 변환('YYYY-MM-DD HH:MM:SS' 형식으로)
string LogStorageManager::convertTimestamp(const string& rawTimestamp, bool bIsAudit)
{
    if (bIsAudit)
    { // audit.log의 timestamp(unix time)인 경우 e.g. "1364481363.243"
        time_t rawTime = static_cast<time_t>(stod(rawTimestamp));
        struct tm* timeInfo = localtime(&rawTime); // 시스템 설정에 따른 시간대로 변환
        char buffer[20];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeInfo);
        return string(buffer);
    }
    else
    { // rsyslog의 timestamp(RFC time)인 경우 e.g. "2025-05-19T01:19:16.320023+09:00"
        string trimmed = rawTimestamp.substr(0, 19); // "2025-05-19T01:19:16"
        for (char& c : trimmed)
        {
            if (c == 'T')
                c = ' '; // "2025-05-19 01:19:16"
        }
        return trimmed;
    }
}