#pragma once

#include <string>       // 문자열 클래스
#include <optional>     // std::optional 사용
#include <unordered_set>

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

    void RsyslogRun();

private:
    std::string mLogPath;
    std::unordered_set<std::string> mRsyslogRuleSet;

    std::unordered_set<std::string> LoadRsyslogRuleSet(const std::string& filename);
    std::optional<LogEntry> ParseLogLine(const std::string& line);
};
