#include "RsyslogManager.h"
#include "RsyslogRule.h"
#include "LogStorageManager.h"
#include "Paths.h"

#include <iostream>
#include <fstream>
#include <regex>
#include <thread>
#include <chrono>
#include <spdlog/spdlog.h>
#include <yaml-cpp/yaml.h>

RsyslogManager::RsyslogManager()
    : mLogPath(PATH_LOG)
    , mRsyslogRuleSet(loadRsyslogRuleSet(PATH_RULESET))
{}

void RsyslogManager::Init(std::atomic<bool>& shouldRun)
{
    mpShouldRun = &shouldRun;
}

// 로그 분석을 위한 timestamp 변환
time_t RsyslogManager::ParseTime(const std::string& timestamp)
{
    std::string trimmed = timestamp.substr(0, 19); // 필요없는 부분 자르기
    struct tm tm{};
    strptime(trimmed.c_str(), "%Y-%m-%dT%H:%M:%S", &tm);
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
        //std::cerr << "[ERROR] YAML parsing error: " << e.what() << "\n";
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
        return LogEntry{ match[1], match[2], "unknown", match[3], match[4], line };
    }

    return std::nullopt;
}

// 로그 모니터링 실행
void RsyslogManager::Run()
{
    std::ifstream file(mLogPath);

    if (!file.is_open())
    {
        spdlog::error("로그 파일 열기 실패: {}", mLogPath);
        return;
    }

    file.seekg(0, std::ios::end);

    std::string line;

    while (*mpShouldRun)
    {
        if (std::getline(file, line))
        {
            auto entry = parseLogLine(line);
            if (entry)
            {
                mRecentLogs.push_back(*entry);

                // 개수 기준으로 오래된 로그 삭제 (10개 초과 시 제거)
                if (mRecentLogs.size() > MAX_RECENT_LOGS)
                {
                    mRecentLogs.pop_front();
                }

                std::vector<std::function<AnalysisResult()>> analyzers =
                {
                    [&] { return AnalyzePasswordFailureLog(*entry); },
                    [&] { return AnalyzeSudoLog(*entry, mRsyslogRuleSet["sudousers"]); },
                    [&] { return AnalyzePasswdChangeLog(*entry, mRecentLogs, mRsyslogRuleSet["passwdchangers"]); },
                    [&] { return AnalyzeSudoGroupChangeLog(*entry, mRecentLogs); },
                    [&] { return AnalyzeUserChangeLog(*entry, mRecentLogs); },
                    [&] { return AnalyzeGroupChangeLog(*entry, mRecentLogs); },
                    [&] { return AnalyzeGroupMemberChangeLog(*entry, mRecentLogs); }
                };

                AnalysisResult result;
                for (const auto& analyzer : analyzers)
                {
                    result = analyzer();
                    if (result.isMalicious)
                    {
                        if (!result.username.empty())
                        {
                            entry->username = result.username;
                        }
                        break;
                    }
                }

                if (result.isMalicious)
                {
                    spdlog::warn("악성 로그 탐지됨: [{}] {} - {}", 
                                 result.type, result.username, result.description);
                    //DB 저장
                    LogStorageManager manager;
                    
                    LogAnalysisResult lar;
                    lar.type = result.type;
                    lar.description = result.description;
                    lar.timestamp = entry->timestamp;
                    lar.username = entry->username;
                    lar.originalLogPath = mLogPath;
                    lar.rawLine = entry->raw;
                    manager.Run(lar,false);
                }
                else 
                {
                    spdlog::debug("정상 로그 처리됨: {}", entry->raw);
                }
            }
            else
            {
                spdlog::debug("로그 파싱 실패 또는 무시됨: {}", line);
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
                spdlog::error("로그 파일 읽기 중 오류 발생");
                break;
            }
        }
    }
}
