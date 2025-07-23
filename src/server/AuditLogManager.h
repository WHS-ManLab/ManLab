#pragma once

#include <string>
#include <map>
#include <vector>
#include <fstream>
#include <atomic>

struct Condition
{
public:
    std::string Field;
    std::string Value;
    std::string MatchType;
};

struct AuditLogRule
{
public:
    std::string Type;
    std::string Key;
    std::string Description;
    std::vector<Condition> Conditions;
};

struct AuditLogRecord
{
public:
    std::string MsgId;
    std::map<std::string, std::string> Fields;
    bool bHasExecve = false;
    bool bHasSyscall = false;
    bool bHasPath = false;
    std::string RawLine;
};

class AuditLogManager
{
public:
    void LoadRules();
    std::string ExtractMsgId(const std::string& line) const;
    std::string AuidToUsername(const std::string& auidStr);
    bool LogMonitor(std::ifstream& infile);
    void Run();
    void Init(std::atomic<bool>& shouldRun);

private:
    std::vector<AuditLogRule> mRules;
    std::map<std::string, AuditLogRecord> mRecords;

    bool parseLogLine(const std::string& line, AuditLogRecord& record);
    std::map<std::string, std::string> parseKeyValue(const std::string& line);
    bool matches(const AuditLogRecord& record, const AuditLogRule& rule) const;

    std::atomic<bool>* mpShouldRun = nullptr;
};
