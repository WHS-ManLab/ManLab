#pragma once

#include <string>
#include <optional>
#include <deque>
#include <unordered_set>
#include <unordered_map>

// 파싱된 로그 항목을 담는 구조체
struct LogEntry
{
    std::string timestamp;
    std::string hostname;
    std::string process;
    std::string message;
    std::string raw;
};

// 악성 행위 분석 결과 구조체
struct AnalysisResult
{
    bool isMalicious;
    std::string type;
    std::string description;
};

class RsyslogManager
{
public:
    RsyslogManager(const std::string& logPath, const std::string& ruleSetPath);
    
    static time_t ParseTime(const std::string& timestamp);
    void RsyslogRun();

private:
    static constexpr size_t MAX_RECENT_LOGS = 10;

    std::string mLogPath;
    std::unordered_map<std::string, std::unordered_set<std::string>> mRsyslogRuleSet;

    std::deque<LogEntry> mRecentLogs;
    std::unordered_map<std::string, std::unordered_set<std::string>> loadRsyslogRuleSet(const std::string& filename);
    std::optional<LogEntry> parseLogLine(const std::string& line);
};
