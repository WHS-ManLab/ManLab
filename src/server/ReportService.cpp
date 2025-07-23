#include "ReportService.h"
#include "INIReader.h"
#include "StringUtils.h"
#include "GmailClient.h"
#include "Paths.h"
#include "DBManager.h"

#include <sstream>    // istringstream
#include <fstream>    // ofstream
#include <cstdlib>    // close, system
#include <chrono>     // system_clock 등
#include <thread>     // sleep
#include <spdlog/spdlog.h>

using namespace std::chrono;
using namespace manlab::utils;

bool ReportService::loadEmailSettings()
{
    INIReader reader(PATH_LOG_REPORT_INI);
    if (reader.ParseError() != 0)
    {
        spdlog::warn("리포트 INI 파싱 실패");
        return false;
    }

    std::string typeStr = reader.Get("Report", "Type", "");
    std::string timeStr = reader.Get("Report", "Time", "");
    spdlog::debug("typeStr {}, timestr {}", typeStr, timeStr);

    GeneralSchedule schedule;
    bool parsed = ParseScheduleFromINI("Report", typeStr, timeStr, schedule);
    if (parsed)
    {
        mStartTime = getReportStartTime(schedule);
        spdlog::info("mStartTime {}", mStartTime);

        std::string strEnabled = trim(stripComment(reader.Get("Email", "Enabled", "false")));
        mbIsEnabled = (strEnabled == "true");

        mRecipient = trim(stripComment(reader.Get("Email", "Recipient", "")));

        return true;
    }
    else
    {
        return false;
    }
}

bool ReportService::Run()
{
    if(!loadEmailSettings())
    {
        spdlog::info("Failed to load EmailSettings.");
        return false;
    }

    mEndTime = getCurrentTimeString();
    std::string htmlFile = std::string(PATH_LOG_REPORT) + "/Report_" + mEndTime.substr(0, 10) + ".html";

    auto events = fetchData(mStartTime, mEndTime);
    if (generateHTML(htmlFile, events))
    {
        if (mbIsEnabled)
        {
            GmailClient client(mRecipient);
            if(client.Run(htmlFile))
            {
                return true;
            }
        }
    }
    else
    {
        spdlog::error("Failed to generate HTML.");
    }
    return false;
}

std::vector<LogAnalysisResult> ReportService::fetchData(const std::string &fromTime, const std::string &toTime)
{
    auto &storage = DBManager::GetInstance().GetLogAnalysisResultStorage();
    return storage.get_all<LogAnalysisResult>(
        sqlite_orm::where(sqlite_orm::between(&LogAnalysisResult::timestamp, fromTime, toTime)));
}

std::string ReportService::getReportStartTime(const GeneralSchedule& schedule) const
{
    auto now = system_clock::now();

    if (schedule.type == ScheduleType::Daily)
    {
        now -= hours(24);
    }
    else if (schedule.type == ScheduleType::Weekly)
    {
        now -= hours(24 * 7);
    }
    else if (schedule.type == ScheduleType::Monthly)
    {
        // 한 달 전은 30일 전으로 처리
        now -= hours(24 * 30);
    }

    auto timet = system_clock::to_time_t(now);
    char buf[20];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&timet));
    return buf;
}

std::string ReportService::getCurrentTimeString() const
{
    auto now = system_clock::now();
    auto timetNow = system_clock::to_time_t(now);
    char buf[20];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&timetNow));
    return buf;
}

bool ReportService::generateHTML(const std::string &htmlFile, const std::vector<LogAnalysisResult> &events) const
{
    std::ofstream html(htmlFile);
    if (!html)
    {
        return false;
    }

    // --------------------------------------------------
    // LOG 팀 리포트
    html << R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>Malicious Behavior Report</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <script src="https://cdn.jsdelivr.net/npm/chartjs-plugin-datalabels@2"></script>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        table { border-collapse: collapse; width: 100%; }
        th, td { border: 1px solid #ccc; padding: 8px; text-align: left; }
        th { background-color: #f2f2f2; }
    </style>
</head>
<body>
<h1>Malicious Behavior Report</h1>
<p>Report period: )";

    html << mStartTime << " ~ " << mEndTime  << "</p>\n";

    html << R"(
<h2>Detected Malicious Behavior Types Overview</h2>)";
    html << R"(<canvas id="typeDonutChart" width="400" height="400"></canvas>)";

    html << R"(
<h2>Detected Malicious Behavior Details</h2>
<table>
    <thead>
        <tr>
            <th>Event ID</th>
            <th>Type</th>
            <th>Description</th>
            <th>Timestamp</th>
            <th>Username</th>
            <th>Original Log Path</th>
        </tr>
    </thead>
    <tbody>
)";

    if (events.empty())
    {
        html << R"(<tr>
            <td colspan="7" style="text-align: center; font-style: italic;">
            No malicious behavior events detected during this period.
            </td>
        </tr>
    </tbody>
</table>
)";
    }
    else
    {
        std::map<std::string, int> typeCounts;
        int id = 1;

        for (const auto &e : events)
        {
            typeCounts[e.type]++;

            html << "<tr>";
            html << "<td>" << id++ << "</td>";
            html << "<td>" << e.type << "</td>";
            html << "<td>" << e.description << "</td>";
            html << "<td>" << e.timestamp << "</td>";
            html << "<td>" << e.username << "</td>";
            html << "<td>" << e.originalLogPath << "</td>";
            html << "</tr>\n";
        }

        html << R"(</tbody>
</table>

<h2>Event Log Line by ID</h2>
<table>
    <thead>
        <tr>
            <th>Event ID</th>
            <th>Raw Log Line</th>
        </tr>
    </thead>
    <tbody>
)";

        id = 1;
        for (const auto &e : events)
        {
            html << "<tr>";
            html << "<td>" << id++ << "</td>";
            html << "<td>" << e.rawLine << "</td>";
            html << "</tr>\n";
        }

        html << R"(</tbody>
</table>
)";

        std::string labelsStr, dataStr, colorStr;
        bool first = true;
        size_t colorIdx = 0;

        for (const auto &pair : typeCounts)
        {
            if (!first)
            {
                labelsStr += ", ";
                dataStr += ", ";
                colorStr += ", ";
            }
            labelsStr += "\"" + pair.first + "\"";
            dataStr += std::to_string(pair.second);
            colorStr += "\"" + ReportService::generateColor(colorIdx) + "\"";
            colorIdx++;
            first = false;
        }

        html << R"(
<script>
const ctx = document.getElementById('typeDonutChart').getContext('2d');
new Chart(ctx, {
    type: 'doughnut',
    data: {
        labels: [)";
        html << labelsStr;
        html << R"(],
        datasets: [{
            data: [)";
        html << dataStr;
        html << R"(],
            backgroundColor: [)";
        html << colorStr;
        html << R"(]
        }]
    },
    options: {
        responsive: false,
        plugins: {
            legend: { position: 'bottom' },
            datalabels: {
                color : '#000',
                font: { weight: 'bold' },
                formatter: (value) => value
            }
        }
    },
    plugins: [ChartDataLabels]
});
</script>
)";
    }

    // -------------------------------------------------------
    // FIM 팀 리포트
    // 수동검사
    html << R"(
<h1>📂 File Integrity Monitoring Report</h1>

<h2>Modified Files (Manual Scan Results)</h2>
<p>List of files with changed MD5 hashes detected during manual scans:</p>
<canvas id="manualScanChart" width="400" height="400"></canvas>
<table>
    <thead>
        <tr>
            <th>Path</th>
            <th>Current MD5 Hash</th>
            <th>Permission</th>
            <th>UID</th>
            <th>GID</th>
            <th>CTime</th>
            <th>MTime</th>
            <th>Size</th>
        </tr>
    </thead>
    <tbody>
)";

// 수동검사 데이터 수집
std::vector<ModifiedEntry> modifiedRecords;
try {
    auto& modifiedStorage = DBManager::GetInstance().GetModifiedStorage();
    modifiedRecords = modifiedStorage.get_all<ModifiedEntry>();
} catch (const std::exception& e) {
    html << "<tr><td colspan='2' style='color:red;'>Error: " << e.what() << "</td></tr>";
}

std::map<std::string, int> manualTypeCounts;

if (modifiedRecords.empty()) {
    html << R"(<tr>
        <td colspan="2" style="text-align: center; font-style: italic;">
        No file integrity changes detected during manual scans.
        </td>
    </tr>
)";
} else {
    for (const auto& record : modifiedRecords) {
        html << "<tr>";
        html << "<td>" << record.path << "</td>";
        html << "<td>" << record.current_md5 << "</td>";
        html << "<td>" << record.current_permission << "</td>";
        html << "<td>" << record.current_uid << "</td>";
        html << "<td>" << record.current_gid << "</td>";
        html << "<td>" << record.current_ctime << "</td>";
        html << "<td>" << record.current_mtime << "</td>";
        html << "<td>" << record.current_size << "</td>";
        html << "</tr>\n";
        std::string ext = record.path.substr(record.path.find_last_of('.') + 1);
        manualTypeCounts[ext]++;
    }
}
html << R"(</tbody></table>)";

// 실시간 검사
html << R"(
<h2>Real-time Monitoring Events</h2>
<p>Events from )" << mStartTime << " to " << mEndTime << R"(</p>
<canvas id="realtimeChart" width="400" height="400"></canvas>
<table>
    <thead>
        <tr>
            <th>ID</th>
            <th>Path</th>
            <th>Event Type</th>
            <th>Timestamp</th>
        </tr>
    </thead>
    <tbody>
)";

// 실시간 검사 데이터 수집
std::vector<RealtimeEventLog> realTimeRecords;
std::map<std::string, int> eventTypeCounts;

try {
    auto& realTimeStorage = DBManager::GetInstance().GetRealTimeMonitorStorage();
    realTimeRecords = realTimeStorage.get_all<RealtimeEventLog>(
        sqlite_orm::where(sqlite_orm::between(&RealtimeEventLog::timestamp, mStartTime, mEndTime)));
} catch (const std::exception& e) {
    html << "<tr><td colspan='4' style='color:red;'>Error: " << e.what() << "</td></tr>";
}

if (realTimeRecords.empty()) {
    html << R"(<tr>
        <td colspan="4" style="text-align: center; font-style: italic;">
        No real-time monitoring events detected during this period.
        </td>
    </tr>
)";
} else {
    for (const auto& record : realTimeRecords) {
        html << "<tr><td>" << record.id << "</td><td>" << record.path
             << "</td><td>" << record.eventType << "</td><td>" << record.timestamp << "</td></tr>";
        eventTypeCounts[record.eventType]++;
    }
}
html << R"(</tbody></table>)";

// 시각화용 스크립트 추가
// 수동검사 차트
html << R"(
<script>
const manualCtx = document.getElementById('manualScanChart').getContext('2d');
new Chart(manualCtx, {
    type: 'doughnut',
    data: {
        labels: [)";
bool first = true;
for (const auto& [ext, _] : manualTypeCounts) {
    if (!first) html << ", ";
    html << "\"" << ext << "\"";
    first = false;
}
html << R"(],
        datasets: [{
            data: [)";
first = true;
for (const auto& [_, count] : manualTypeCounts) {
    if (!first) html << ", ";
    html << count;
    first = false;
}
html << R"(],
            backgroundColor: [)";
for (size_t i = 0; i < manualTypeCounts.size(); ++i) {
    if (i > 0) html << ", ";
    html << "\"" << ReportService::generateColor(i) << "\"";
}
html << R"(]
        }]
    },
    options: {
        responsive: false,
        plugins: {
            legend: { position: 'bottom' },
            datalabels: {
                color : '#000',
                font: { weight: 'bold' },
                formatter: (value) => value
            }
        }
    },
    plugins: [ChartDataLabels]
});

// 실시간 검사 차트
const rtCtx = document.getElementById('realtimeChart').getContext('2d');
new Chart(rtCtx, {
    type: 'doughnut',
    data: {
        labels: [)";
first = true;
for (const auto& [type, _] : eventTypeCounts) {
    if (!first) html << ", ";
    html << "\"" << type << "\"";
    first = false;
}
html << R"(],
        datasets: [{
            data: [)";
first = true;
for (const auto& [_, count] : eventTypeCounts) {
    if (!first) html << ", ";
    html << count;
    first = false;
}
html << R"(],
            backgroundColor: [)";
for (size_t i = 0; i < eventTypeCounts.size(); ++i) {
    if (i > 0) html << ", ";
    html << "\"" << ReportService::generateColor(i) << "\"";
}
html << R"(]
        }]
    },
    options: {
        responsive: false,
        plugins: {
            legend: { position: 'bottom' },
            datalabels: {
                color : '#000',
                font: { weight: 'bold' },
                formatter: (value) => value
            }
        }
    },
    plugins: [ChartDataLabels]
});
</script>
)";

    // -------------------------------------------------------
    // SIG 팀 리포트
    html << R"(
<h1>Malware Scan Report</h1>
<p>Scan records from )" << mStartTime << " to " << mEndTime << R"(</p>
<table>
    <thead>
        <tr>
            <th>ID</th>
            <th>Type</th>
            <th>Date</th>
            <th>Detected</th>
        </tr>
    </thead>
    <tbody>
)";

    auto& scanStorage = DBManager::GetInstance().GetScanReportStorage();
    auto scanReports = scanStorage.get_all<ScanReport>(
        sqlite_orm::where(sqlite_orm::between(&ScanReport::date, mStartTime, mEndTime)));

    if (scanReports.empty()) {
        html << R"(<tr>
            <td colspan="4" style="text-align: center; font-style: italic;">
            No scan data found during this period.
            </td>
        </tr>
    </tbody>
</table>
)";
    } else {
        for (const auto& report : scanReports) {
            html << "<tr>";
            html << "<td>" << report.id << "</td>";
            html << "<td>" << report.type << "</td>";
            html << "<td>" << report.date << "</td>";
            html << "<td>" << (report.detected ? "Yes" : "No") << "</td>";
            html << "</tr>\n";
        }
        html << R"(</tbody>
</table>
<h2>Full Report Texts</h2>
)";

        for (const auto& report : scanReports) {
            html << "<pre style=\"background:#f9f9f9; padding:10px; border:1px solid #ccc;\">\n";
            html << report.report;
            html << "\n</pre>\n";
        }
    }

    html << R"(
</body>
</html>
)";
    html.close();
    return true;
}

std::string ReportService::generateColor(size_t index)
{
    static const std::vector<std::string> baseColors = {
        "255, 99, 132", "54, 162, 235", "255, 206, 86",
        "75, 192, 192", "153, 102, 255", "255, 159, 64"
    };
    if (index < baseColors.size()) 
    {
        return "rgba(" + baseColors[index] + ", 0.6)";
    } 
    else 
    {
        int r = (index * 50) % 256;
        int g = (index * 80) % 256;
        int b = (index * 110) % 256;
        return "rgba(" + std::to_string(r) + ", " + std::to_string(g) + ", " + std::to_string(b) + ", 0.6)";
    }
}