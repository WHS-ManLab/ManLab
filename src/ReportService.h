#pragma once

#include <string>
#include <vector>
#include <chrono>
#include "DBManager.h"

class ReportService {
public:
    bool Run();

private:
    bool loadFromIni();
    std::vector<LogAnalysisResult> fetchData(const std::string& fromTime, const std::string& toTime);
    std::string getCurrentTimeString() const;
    bool generateHTML(const std::string& htmlFile, const std::vector<LogAnalysisResult>& events) const;
    static std::string generateColor(size_t index);

    std::string mPeriodType;
    int mDayOfWeek;
    int mDayOfMonth;
    int mGenerationHour;
    int mGenerationMinute;
    bool mbIsEnabled;
    std::string mRecipient;

    std::string mCurrentTime;
    std::string mLastReportTime = "2025-07-01 00:00:00"; // test용
};