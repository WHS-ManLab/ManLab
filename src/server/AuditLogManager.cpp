#include "AuditLogManager.h"
#include "LogStorageManager.h"
#include "Paths.h"

#include <iostream>
#include <sstream>
#include <yaml-cpp/yaml.h>
#include <thread>
#include <chrono>

using namespace std;

void AuditLogManager::Init(std::atomic<bool>& shouldRun)
{
    mpShouldRun = &shouldRun;
}

// 로그 라인에서 msg=audit(...) 내부 숫자 추출 (msgId)
string AuditLogManager::ExtractMsgId(const string& line) const
{
    size_t pos = line.find("msg=audit(");

    if (pos == string::npos)
    {
        return "";
    }

    size_t start = pos + 10;
    size_t end = line.find(')', start);

    if (end == string::npos)
    {
        return "";
    }

    return line.substr(start, end - start);
}

// 로그 한 줄 파싱해서 AuditLogRecord에 필요한 정보 저장
bool AuditLogManager::parseLogLine(const string& line, AuditLogRecord& record)
{
    if (line.find("type=SYSCALL") != string::npos)
    {
        record.bHasSyscall = true;
        record.RawLine = line;
    }
    else if (line.find("type=EXECVE") != string::npos)
    {
        record.bHasExecve = true;
    }
    else if (line.find("type=PATH") != string::npos)
    {
        if (line.find("item=1") != string::npos)
        {
            record.bHasPath = true;
        }
    }
    else
    {
        return false;
    }

    istringstream iss(line);
    string token;

    while (iss >> token)
    {
        size_t eqPos = token.find('=');

        if (eqPos == string::npos)
        {
            continue;
        }

        string key = token.substr(0, eqPos);
        string value = token.substr(eqPos + 1);

        if (!value.empty() && value.front() == '"' && value.back() == '"')
        {
            value = value.substr(1, value.size() - 2);
        }

        record.Fields[key] = value;
    }

    return true;
}

// 로그 레코드가 룰 조건에 부합하는지 확인
bool AuditLogManager::Matches(const AuditLogRecord& record, const AuditLogRule& rule) const
{
    for (const Condition& cond : rule.Conditions)
    {
        if (cond.Field == "*") // 모든 필드 대상 조건 검사
        {
            bool bFound = false;

            for (const pair<const string, string>& field : record.Fields)
            {
                if (cond.MatchType == "contains" && field.second.find(cond.Value) != string::npos)
                {
                    bFound = true;
                    break;
                }

                if (cond.MatchType == "notcontains" && field.second.find(cond.Value) == string::npos)
                {
                    bFound = true;
                    break;
                }
            }

            if (!bFound)
            {
                return false;
            }
        }
        else
        {
            map<string, string>::const_iterator it = record.Fields.find(cond.Field);

            if (it == record.Fields.end())
            {
                return false;
            }

            const string& actual = it->second;
            const string& expected = cond.Value;
            const string& mt = cond.MatchType;

            if (mt == "equals" && actual != expected)
            {
                return false;
            }

            if (mt == "notequals" && actual == expected)
            {
                return false;
            }

            if (mt == "contains" && actual.find(expected) == string::npos)
            {
                return false;
            }

            if (mt == "notcontains" && actual.find(expected) != string::npos)
            {
                return false;
            }
        }
    }

    return true;
}

// 룰 불러오기
void AuditLogManager::LoadRules()
{
    const string ruleFile = PATH_AUDITLOGRULES;
    YAML::Node yaml = YAML::LoadFile(ruleFile);

    for (const YAML::Node& ruleNode : yaml)
    {
        AuditLogRule rule;

        rule.Key = ruleNode["key"].as<string>();
        rule.Description = ruleNode["description"].as<string>();

        for (const YAML::Node &condNode : ruleNode["conditions"])
        {
            Condition cond;

            cond.Field = condNode["field"].as<string>();
            cond.Value = condNode["value"].as<string>();
            cond.MatchType = condNode["match_type"].as<string>();

            rule.Conditions.push_back(cond);
        }

        mRules.push_back(rule);
    }
}

// 로그 파일에서 한 줄 읽어 레코드에 저장
bool AuditLogManager::LogMonitor(ifstream& infile)
{
    string line;
    streampos lastPos = infile.tellg();

    if (!getline(infile, line))
    {
        this_thread::sleep_for(chrono::milliseconds(200));
        infile.clear();
        infile.seekg(lastPos);

        return false;
    }

    string msgId = ExtractMsgId(line);

    if (msgId.empty())
    {
        return true;
    }

    AuditLogRecord& record = mRecords[msgId];
    record.MsgId = msgId;

    parseLogLine(line, record);

    return true;
}

void AuditLogManager::Run()
{
    LogStorageManager manager;
    const string auditLogPath = PATH_AUDITLOG;

    LoadRules();

    ifstream infile(auditLogPath);

    if (!infile)
    {
        return;
    }

    infile.seekg(0, ios::end);

    while (*mpShouldRun)
    {
        LogMonitor(infile);

        for (map<string, AuditLogRecord>::iterator it = mRecords.begin(); it != mRecords.end();)
        {
            AuditLogRecord& record = it->second;

            if (record.bHasSyscall && (record.bHasExecve || record.bHasPath))
            {
                for (const AuditLogRule& rule : mRules)
                {
                    if (Matches(record, rule))
                    {
                        LogAnalysisResult lar;

                        lar.type = rule.Key;
                        lar.description = rule.Description;

                        size_t colonPos = record.MsgId.find(':');
                        if (colonPos != std::string::npos)
                            lar.timestamp = record.MsgId.substr(0, colonPos);
                        else
                            lar.timestamp = record.MsgId;

                        lar.username = record.Fields.count("auid") > 0 ? record.Fields.at("auid") : "";
                        lar.originalLogPath = PATH_AUDITLOG;
                        lar.rawLine = record.RawLine;

                        manager.Run(lar, true);
                    }
                }

                it = mRecords.erase(it);
            }
            else
            {
                ++it;
            }
        }

        this_thread::sleep_for(chrono::milliseconds(200));
    }
}
