#pragma once

#include <string>
#include <map>
#include <vector>
#include <fstream>

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
    std::string RawLine;
};

class AuditLogManager
{
public:
    void LoadRules();
    std::string ExtractMsgId(const std::string &line) const;
    bool LogMonitor(std::ifstream &infile);
    void Run();

private:
    std::vector<AuditLogRule> mRules;
    std::map<std::string, AuditLogRecord> mRecords;

    bool parseLogLine(const std::string &line, AuditLogRecord &record);
    bool Matches(const AuditLogRecord &record, const AuditLogRule &rule) const;
};
