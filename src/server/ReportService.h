#pragma once

#include <string>
#include <vector>
#include "DBManager.h"

class ReportService {
public:
    bool Run();

private:
    bool loadEmailSettings(); // 변경됨: ini에서 이메일 관련 정보만 로드
    std::vector<LogAnalysisResult> fetchData(const std::string& fromTime, const std::string& toTime);
    std::string getCurrentTimeString() const;
    bool generateHTML(const std::string& htmlFile, const std::vector<LogAnalysisResult>& events) const;
    static std::string generateColor(size_t index);

    // 이메일 관련 설정만 유지
    bool mbIsEnabled = false;
    std::string mRecipient;

    std::string mCurrentTime;
    std::string mLastReportTime = "2025-07-01 00:00:00"; // 추후 지속 저장 가능
};