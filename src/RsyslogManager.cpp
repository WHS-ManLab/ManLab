#include "RsyslogManager.h"
#include "RsyslogRule.h"
#include "LogStorageManager.h"
#include "LogDaemon.h"

#include <iostream>
#include <fstream>
#include <regex>
#include <thread>
#include <chrono>
#include <ctime>

#include <yaml-cpp/yaml.h>

RsyslogManager::RsyslogManager(const std::string& logPath, const std::string& ruleSetPath)
    : mLogPath(logPath)
    , mRsyslogRuleSet(loadRsyslogRuleSet(ruleSetPath))
{}

// 로그 분석을 위한 timestamp 변환
time_t RsyslogManager::ParseTime(const std::string& timestamp) {
    struct tm tm{};
    strptime(timestamp.c_str(), "%b %d %H:%M:%S", &tm);
    
    // 연도, 월 기본값 보정 (옵션)
    time_t now = time(nullptr);
    struct tm* now_tm = localtime(&now);
    tm.tm_year = now_tm->tm_year;
    tm.tm_mon = now_tm->tm_mon;

    return mktime(&tm);
}

// RsyslogRuleSet 파싱
std::unordered_map<std::string, std::unordered_set<std::string>> RsyslogManager::loadRsyslogRuleSet(const std::string& filename)
{
    std::unordered_map<std::string, std::unordered_set<std::string>> result;

    try
    {
        YAML::Node config = YAML::LoadFile(filename);

        for (auto it = config.begin(); it != config.end(); ++it) {
            std::string ruleKey = it->first.as<std::string>();
            const YAML::Node& ruleList = it->second;

            if (ruleList.IsSequence()) {
                for (const auto& item : ruleList) {
                    result[ruleKey].insert(item.as<std::string>());
                }
            }
        }
    }
    catch (const YAML::Exception& e)
    {
        std::cerr << "[ERROR] YAML parsing error: " << e.what() << "\n";
    }

    return result;
}

// 로그 한 줄을 파싱하여 LogEntry 구조로 반환
std::optional<LogEntry> RsyslogManager::parseLogLine(const std::string& line)
{
    std::regex oldFmt(R"((\w{3}\s+\d+\s[\d:]+)\s(\S+)\s([^\s:]+):\s(.+))");
    std::regex newFmt(R"((\d{4}-\d{2}-\d{2}T[\d:.+-]+)\s(\S+)\s(\S+):\s(.+))");

    std::smatch match;

    if (std::regex_match(line, match, oldFmt) || std::regex_match(line, match, newFmt))
    {
        return LogEntry{ match[1], match[2], match[3], match[4], line };
    }

    return std::nullopt;
}

// 로그 모니터링 실행
void RsyslogManager::RsyslogRun()
{
    std::ifstream file(mLogPath);

    if (!file.is_open())
    {
        std::cerr << "[ERROR] Failed to open log file: " << mLogPath << std::endl;
        return;
    }

    file.seekg(0, std::ios::end);

    std::string line;

    while (LogCollectorDaemon::IsRunning())
    {
        if (std::getline(file, line))
        {
            auto entry = parseLogLine(line);
            if (entry)
            {
                //추가해야됨
                mRecentLogs.push_back(*entry);

                //추가해야됨
                // 개수 기준으로 오래된 로그 삭제 (10개 초과 시 제거)
                if (mRecentLogs.size() > MAX_RECENT_LOGS) {
                    mRecentLogs.pop_front();
                }

                AnalysisResult result{false, "", ""};

                //수정 및 추가 해야됨
                result = AnalyzePasswordFailureLog(*entry);

                if (!result.isMalicious)
                {
                    result = AnalyzeSudoLog(*entry, mRsyslogRuleSet["sudousers"]);
                }
                if (!result.isMalicious)
                {
                    result = AnalyzePasswdChangeLog(*entry, mRecentLogs, mRsyslogRuleSet["passwdchangers"]);
                }

                if (result.isMalicious)
                {
                    //DB 저장
                    LogStorageManager manager;
                    
                    LogAnalysisResult lar;
                    lar.type = result.type;
                    lar.description = result.description;
                    lar.timestamp = entry->timestamp;
                    lar.uid = entry->hostname;
                    lar.bIsSuccess = true;
                    lar.originalLogPath = mLogPath;
                    lar.rawLine = entry->raw;

                    manager.Run(lar,false);
                }
            }
        }
        else
        {
            if (file.eof())
            {
                file.clear();
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
            else
            {
                std::cerr << "[ERROR] Log file read error\n";
                break;
            }
        }
    }
}
