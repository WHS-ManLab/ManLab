#pragma once

#include <string>
#include <unordered_set>

#include "RsyslogManager.h"  // LogEntry, AnalysisResult 사용

AnalysisResult AnalyzeSudoLog(const LogEntry& entry, const std::unordered_set<std::string>& rsyslogRuleSet);
AnalysisResult AnalyzePasswdChangeLog(const LogEntry& entry, const std::deque<LogEntry>& recentLogs, const std::unordered_set<std::string>& rsyslogRuleSet);
AnalysisResult AnalyzePasswordFailureLog(const LogEntry& entry);
AnalysisResult AnalyzeSudoGroupChangeLog(const LogEntry& entry, const std::deque<LogEntry>& recentLogs);
AnalysisResult AnalyzeUserChangeLog(const LogEntry& entry, const std::deque<LogEntry>& recentLogs);
AnalysisResult AnalyzeGroupChangeLog(const LogEntry& entry, const std::deque<LogEntry>& recentLogs);
AnalysisResult AnalyzeGroupMemberChangeLog(const LogEntry& entry, const std::deque<LogEntry>& recentLogs);