#pragma once

#include <string>
#include <unordered_set>

#include "RsyslogManager.h"  // LogEntry, AnalysisResult 사용

AnalysisResult AnalyzeSudoLog(const LogEntry& entry, const std::unordered_set<std::string>& rsyslogRuleSet);
