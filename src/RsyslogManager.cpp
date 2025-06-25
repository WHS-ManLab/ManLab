#include "RsyslogManager.h"
#include "RsyslogRule.h"
#include "LogStorageManager.h"

#include <iostream>
#include <fstream>
#include <regex>
#include <thread>
#include <chrono>

#include <yaml-cpp/yaml.h>

RsyslogManager::RsyslogManager(const std::string& logPath, const std::string& ruleSetPath)
    : mLogPath(logPath)
    , mRsyslogRuleSet(LoadRsyslogRuleSet(ruleSetPath))
{
}

// RsyslogRuleSet 파싱
std::unordered_set<std::string> RsyslogManager::LoadRsyslogRuleSet(const std::string& filename)
{
    std::unordered_set<std::string> result;

    try
    {
        YAML::Node config = YAML::LoadFile(filename);

        if (!config["sudousers"] || !config["sudousers"].IsSequence())
        {
            std::cerr << "[ERROR] Invalid YAML RsyslogRuleSet format\n";
            return result;
        }

        for (const auto& user : config["sudousers"])
        {
            result.insert(user.as<std::string>());
        }
    }
    catch (const YAML::Exception& e)
    {
        std::cerr << "[ERROR] YAML parsing error: " << e.what() << "\n";
    }

    return result;
}

// 로그 한 줄을 파싱하여 LogEntry 구조로 반환
std::optional<LogEntry> RsyslogManager::ParseLogLine(const std::string& line)
{
    std::regex oldFmt(R"((\w{3}\s+\d+\s[\d:]+)\s(\S+)\s([\w\-]+):\s(.+))");
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

    while (true)
    {
        if (std::getline(file, line))
        {
            auto entry = ParseLogLine(line);
            if (entry)
            {
                auto result = AnalyzeSudoLog(*entry, mRsyslogRuleSet);

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
