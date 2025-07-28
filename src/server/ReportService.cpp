#include "ReportService.h"
#include "INIReader.h"
#include "StringUtils.h"
#include "GmailClient.h"
#include "Paths.h"
#include "DBManager.h"
#include "ScheduleParser.h"

#include <sstream>    // istringstream
#include <fstream>    // ofstream
#include <cstdlib>    // close, system
#include <chrono>     // system_clock 등
#include <thread>     // sleep
#include <map>
#include <spdlog/spdlog.h>
#include <numeric>
#include <iomanip>

using namespace std::chrono;
using namespace manlab::utils;

// "YYYY-MM-DD HH:MM:SS" -> "YYYYMMDD_HHMMSS" 형식으로 변환 (QuarantineMetadata 쿼리에 필요)
std::string convertToQuarantineDateFormat(const std::string& dateTimeStr)
{
    if (dateTimeStr.length() != 19)
    {
        return dateTimeStr;
    }

    std::string year = dateTimeStr.substr(0, 4);
    std::string month = dateTimeStr.substr(5, 2);
    std::string day = dateTimeStr.substr(8, 2);
    std::string hour = dateTimeStr.substr(11, 2);
    std::string minute = dateTimeStr.substr(14, 2);
    std::string second = dateTimeStr.substr(17, 2);

    return year + month + day + "_" + hour + minute + second;
}

// 파일 경로에서 파일 이름만 추출하는 함수
std::string getFileNameFromPath(const std::string& filePath)
{
    size_t lastSlashPos = filePath.find_last_of("/\\");
    if (std::string::npos != lastSlashPos)
    {
        return filePath.substr(lastSlashPos + 1);
    }
    return filePath;
}

std::string generalizeReason(const std::string& quarantineReason)
{
    if (quarantineReason == "md5" || quarantineReason == "sha1" || quarantineReason == "sha256")
    {
        return "Hash";
    }
    else if (quarantineReason == "yara")
    {
        return "YARA";
    }
    return quarantineReason;
}

// QuarantineDate ("YYYYMMDD_HHMMSS")를 "yyyy-mm-dd hh:mm:ss" 형식으로 변환하는 함수
std::string formatQuarantineDateForDisplay(const std::string& quarantineDate)
{
    if (quarantineDate.length() != 15)
    {
        return quarantineDate; 
    }

    std::string year = quarantineDate.substr(0, 4);
    std::string month = quarantineDate.substr(4, 2);
    std::string day = quarantineDate.substr(6, 2);
    std::string hour = quarantineDate.substr(9, 2);
    std::string minute = quarantineDate.substr(11, 2);
    std::string second = quarantineDate.substr(13, 2);

    return year + "-" + month + "-" + day + " " + hour + ":" + minute + ":" + second;
}

bool ReportService::loadEmailSettings()
{
    INIReader reader(PATH_REPORT_INI);
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

    html << R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>ManLab Regular Report</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <script src="https://cdn.jsdelivr.net/npm/chartjs-plugin-datalabels@2"></script>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        table { border-collapse: collapse; width: 100%; }
        th, td { border: 1px solid #ccc; padding: 8px; text-align: left; }
        th { background-color: #f2f2f2; }
        tr:target {
            background-color: #ffff99;
            transition : background-color 0.3s ease;
        }
        tr.highlight {
            background-color: #ffff99;
        }
        h1 {
            text-align: center;
        }
        canvas {
            display: block;
            margin: 0 auto;
        }
    </style>
</head>
<body>
<h1 style="text-align: center;">ManLab Regular Security Report</h1>
<p style="text-align: right;">Report period: )";
    html << mStartTime << " ~ " << mEndTime << "</p>\n";

    // --------------------------------------------------
    // FIM팀 리포트
    html << R"(
<hr/>
<h1>📂 File Integrity Monitoring Report</h1>

<h2>• Modified Files (Manual Scan Results)</h2>
<div style="display: flex; justify-content: space-between;">
    <div style="display: flex; align-items: center;">
        <canvas id="manualScanExtChart" width="400" height="400"></canvas>
        <div id="manualScanExtLegend" style="margin-left: 20px;"></div>
    </div>

    <canvas id="manualScanReasonChart" width="400" height="400"></canvas>
    <canvas id="manualScanTimeChart"   width="400" height="400"></canvas>
</div>

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

    std::vector<ModifiedEntry> modifiedRecords;
    std::map<std::string, int> manualTypeCounts;
    std::map<std::string, int> manualReasonCounts;
    std::map<std::string, int> manualTimeBuckets; // 추가된 시간대별 통계

    try {
        auto& modifiedStorage = DBManager::GetInstance().GetModifiedStorage();
        modifiedRecords = modifiedStorage.get_all<ModifiedEntry>(
            sqlite_orm::where(sqlite_orm::between(&ModifiedEntry::current_mtime, mStartTime, mEndTime))
        );
    } catch (const std::exception& e) {
        html << "<tr><td colspan='9' style='color:red; text-align: center;'>Error fetching manual scan data: " << e.what() << "</td></tr>";
    }


    if (modifiedRecords.empty()) {
        html << R"(<tr>
            <td colspan="9" style="text-align: center; font-style: italic;">
            No file integrity changes detected during manual scans.
            </td>
        </tr>
)";
    } else {

        auto& baselineStorage = DBManager::GetInstance().GetBaselineStorage();
        
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
            if (ext.empty()) ext = "unknown";
            manualTypeCounts[ext]++;

            auto baselineEntryOptional = baselineStorage.get_optional<BaselineEntry>(record.path);

            if (baselineEntryOptional) {
                const auto& baseline = *baselineEntryOptional;

                if (record.current_md5 != baseline.md5) manualReasonCounts["해시값 변경"]++;
                if (record.current_permission != baseline.permission) manualReasonCounts["권한 변경"]++;
                if (record.current_uid != baseline.uid) manualReasonCounts["UID 변경"]++;
                if (record.current_gid != baseline.gid) manualReasonCounts["GID 변경"]++;
                if (record.current_ctime != baseline.ctime) manualReasonCounts["CTime 변경"]++;
                if (record.current_mtime != baseline.mtime) manualReasonCounts["MTime 변경"]++;
                if (record.current_size != baseline.size) manualReasonCounts["크기 변경"]++;
            }
            // [추가] 3시간 단위 시간대별 통계 처리 시작
            {
                std::tm tm = {};
                std::istringstream ssTime(record.current_mtime);
                ssTime >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
                if (!ssTime.fail()) {
                    int bucketStart = (tm.tm_hour / 3) * 3;
                    char bufTime[32];
                    // "YYYY-MM-DD HH:MM" 형식의 라벨 생성
                    std::snprintf(bufTime, sizeof(bufTime), "%s %02d:00",
                                record.current_mtime.substr(0,10).c_str(),
                                bucketStart);
                    manualTimeBuckets[bufTime]++;
                }
            }
        }
    }
    std::vector<std::string> allTimeLabels;
    std::vector<int>         allTimeCounts;
    {
        std::tm t0 = {}, t1 = {};
        std::istringstream(mStartTime) >> std::get_time(&t0, "%Y-%m-%d %H:%M:%S");
        std::istringstream(mEndTime)   >> std::get_time(&t1, "%Y-%m-%d %H:%M:%S");
        auto start = std::mktime(&t0);
        auto end   = std::mktime(&t1);

        // 시작 시각을 가장 가까운 3시간 경계로 내림
        t0 = *std::localtime(&start);
        t0.tm_hour = (t0.tm_hour / 3) * 3;
        t0.tm_min = t0.tm_sec = 0;
        start = std::mktime(&t0);

        // start부터 end까지 3시간씩 레이블과 카운트(없으면 0) 채움
        for (auto tt = start; tt <= end; tt += 3 * 3600) {
            std::tm tb = *std::localtime(&tt);
            char buf[32];
            std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:00", &tb);
            std::string lbl(buf);
            allTimeLabels.push_back(lbl);
            allTimeCounts.push_back(manualTimeBuckets[lbl]);
        }
    }
    html << R"(</tbody></table>)";

    

    html << R"(
<h2>• Real-time Monitoring Events</h2>
<div style="display: flex; justify-content: space-between;">
    <canvas id="realtimeChart"     width="400" height="400"></canvas>
    <canvas id="realtimeTimeChart" width="400" height="400"></canvas>
</div>
<table>
    <thead>
        <tr>
            <th>ID</th>
            <th>Path</th>
            <th>Event Type</th>
            <th>New Name</th>
            <th>Timestamp</th>
        </tr>
    </thead>
    <tbody>
)";

    std::vector<RealtimeEventLog> realTimeRecords;
    std::map<std::string, int> eventTypeCounts;

    try {
        auto& realTimeStorage = DBManager::GetInstance().GetRealTimeMonitorStorage();
        realTimeRecords = realTimeStorage.get_all<RealtimeEventLog>(
            sqlite_orm::where(sqlite_orm::between(&RealtimeEventLog::timestamp, mStartTime, mEndTime)));
    } catch (const std::exception& e) {
        html << "<tr><td colspan='4' style='color:red; text-align: center;'>Error fetching real-time monitoring data: " << e.what() << "</td></tr>";
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
                 << "</td><td>" << record.eventType << "</td><td>" << record.newName << "</td><td>" << record.timestamp << "</td></tr>";
            eventTypeCounts[record.eventType]++;
        }
    }
    html << R"(</tbody></table>)";

    // 3시간 단위 리얼타임 이벤트 통계 계산 시작
    std::map<std::string,int> realTimeTimeBuckets;
    for (const auto& rec : realTimeRecords) {
        std::tm tm{};
        std::istringstream ss(rec.timestamp);
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
        if (!ss.fail()) {
            int bucket = (tm.tm_hour / 3) * 3;
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%s %02d:00",
                          rec.timestamp.substr(0, 10).c_str(),
                          bucket);
            realTimeTimeBuckets[buf]++;
        }
    }

    std::vector<std::string> eventTimeLabels;
    std::vector<int>         eventTimeCounts;
    {
        std::tm t0{}, t1{};
        std::istringstream(mStartTime) >> std::get_time(&t0, "%Y-%m-%d %H:%M:%S");
        std::istringstream(mEndTime)   >> std::get_time(&t1, "%Y-%m-%d %H:%M:%S");
        auto start = std::mktime(&t0);
        auto end   = std::mktime(&t1);

        t0 = *std::localtime(&start);
        t0.tm_hour = (t0.tm_hour / 3) * 3;
        t0.tm_min = t0.tm_sec = 0;
        start = std::mktime(&t0);

        for (auto tt = start; tt <= end; tt += 3 * 3600) {
            std::tm tb = *std::localtime(&tt);
            char lblBuf[32];
            std::strftime(lblBuf, sizeof(lblBuf), "%Y-%m-%d %H:00", &tb);
            std::string lbl(lblBuf);
            eventTimeLabels.push_back(lbl);
            eventTimeCounts.push_back(realTimeTimeBuckets[lbl]);
        }
    }

    // FIM 차트 스크립트
    html << R"(
<script>
const extCtx = document.getElementById('manualScanExtChart').getContext('2d');
const extChart = new Chart(extCtx, {
    type: 'doughnut',
    data: {
        labels: [)";
    bool firstChartItem = true;
    for (const auto& [ext, _] : manualTypeCounts) {
        if (!firstChartItem) html << ", ";
        html << "\"" << ext << "\"";
        firstChartItem = false;
    }
    html << R"(],
        datasets: [{
            data: [)";
    firstChartItem = true;
    for (const auto& [_, count] : manualTypeCounts) {
        if (!firstChartItem) html << ", ";
        html << count;
        firstChartItem = false;
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
            },
            title: {
                display: true,
                text: "File Extension",
                font: {size: 16, weight: 'blod' }
            }
        }
    },
    plugins: [ChartDataLabels] 
});

//도넛 요약 차트 표

(function() {
    const data = extChart.data;
    const total = data.datasets[0].data.reduce((sum, v) => sum + v, 0);
    let htmlLegend = '<table style="border-collapse: collapse; text-align: right;">';
    data.labels.forEach((label, i) => {
        const count = data.datasets[0].data[i];
        const pct   = ((count/total)*100).toFixed(2) + '%';
        const color = data.datasets[0].backgroundColor[i];
        htmlLegend += `
            <tr>
              <td style="padding:4px;">
                <span style="display:inline-block;width:12px;height:12px;
                          background-color:${color};margin-right:8px;"></span>
                ${label}
              </td>
              <td style="padding:4px;">${count}</td>
              <td style="padding:4px;">${pct}</td>
            </tr>`;
    });
    htmlLegend += '</table>';
    document.getElementById('manualScanExtLegend').innerHTML = htmlLegend;
})();

const reasonCtx = document.getElementById('manualScanReasonChart').getContext('2d');
new Chart(reasonCtx, {
    type: 'bar',
    data: {
        labels: [)";
firstChartItem = true;
for (const auto& [reason, _] : manualReasonCounts) {
    if (!firstChartItem) html << ", ";
    html << "\"" << reason << "\"";
    firstChartItem = false;
}
html << R"(],
        datasets: [{
            label: '변경된 파일 수',
            data: [)";
firstChartItem = true;
for (const auto& [_, count] : manualReasonCounts) {
    if (!firstChartItem) html << ", ";
    html << count;
    firstChartItem = false;
}
html << R"(],
            backgroundColor: [)";
for (size_t i = 0; i < manualReasonCounts.size(); ++i) {
    if (i > 0) html << ", ";
    html << "\"" << ReportService::generateColor(i) << "\"";
}
html << R"(]
        }]
    },
    options: {
        responsive: false,
        plugins: {
            legend: { display: false },
            datalabels: {
                color : '#000',
                font: { weight: 'bold' },
                anchor: 'end',
                align: 'top',
                formatter: (value) => value
            },
            title: {
                display: true,
                text: "File Change Reasons",
                font: {size: 16, weight: 'blod' }
            }
        },
        scales: {
            y:{
                beginAtZero: true,
                ticks:{
                    stepSize: 1,
                    precision: 0
                }
            }
        }
    },
    plugins: [ChartDataLabels]
});

const timeCtx = document.getElementById('manualScanTimeChart').getContext('2d');
    new Chart(timeCtx, {
        type: 'bar',
        data: {
            labels: [)";

    for (size_t i = 0; i < allTimeLabels.size(); ++i) {
        if (i) html << ", ";
        html << "\"" << allTimeLabels[i] << "\"";
    }
    html << R"(],
        datasets: [{
            label: '이벤트 수',
            data: [)";
// [바로 여기부터 allTimeCounts 삽입]
    for (size_t i = 0; i < allTimeCounts.size(); ++i) {
        if (i) html << ", ";
        html << allTimeCounts[i];
    }
    html << R"(],
            backgroundColor: [)";
    for (size_t i = 0; i < allTimeCounts.size(); ++i) {
        if (i) html << ", ";
        html << "\"" << ReportService::generateColor(i) << "\"";
    }
    html << R"(]
        }]
    },
    options: {
        responsive: false,
        plugins: {
            legend: { display: false },
            title: {
                display: true,
                text: 'by time events'
            },
            datalabels: {
                color: '#000',
                font: { weight: 'bold' },
                anchor: 'end',
                align: 'top',
                formatter: (v) => v
            }
        },
        scales: {
            x: {
                grid: { display: true, drawBorder: true },
                ticks: { autoSkip: false, maxRotation: 45, minRotation: 45 }
            },
            y: {
                beginAtZero: true,
                grid: { display: true }
            }
        }
    },
    plugins: [ChartDataLabels]
});

const rtCtx = document.getElementById('realtimeChart').getContext('2d');
new Chart(rtCtx, {
    type: 'bar',
    data: {
        labels: [)";
    firstChartItem = true;
    for (const auto& [type, _] : eventTypeCounts) {
        if (!firstChartItem) html << ", ";
        html << "\"" << type << "\"";
        firstChartItem = false;
    }
    html << R"(],
        datasets: [{
            label: 'Event Count',
            data: [)";
    firstChartItem = true;
    for (const auto& [_, count] : eventTypeCounts) {
        if (!firstChartItem) html << ", ";
        html << count;
        firstChartItem = false;
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
        },
        scales: {
            y:{
                beginAtZero: true,
                ticks:{
                    stepSize: 1,
                    precision: 0
                }
            }
        }
    },
    plugins: [ChartDataLabels]
});

const rtTimeCtx = document.getElementById('realtimeTimeChart').getContext('2d');
new Chart(rtTimeCtx, {
    type: 'bar',
    data: {
        labels: [)";
for (size_t i = 0; i < eventTimeLabels.size(); ++i) {
    if (i) html << ", ";
    html << "\"" << eventTimeLabels[i] << "\"";
}
html << R"(],
        datasets: [{
            label: 'Events by Time',
            data: [)";
for (size_t i = 0; i < eventTimeCounts.size(); ++i) {
    if (i) html << ", ";
    html << eventTimeCounts[i];
}
html << R"(],
            backgroundColor: [)";
for (size_t i = 0; i < eventTimeCounts.size(); ++i) {
    if (i) html << ", ";
    html << "\"" << ReportService::generateColor(i) << "\"";
}
html << R"(
            ]
        }]
    },
    options: {
        responsive: false,
        plugins: {
            legend: { display: false },
            title: {
                display: true,
                text: 'Events by Time'
            },
            datalabels: {
                color: '#000',
                font: { weight: 'bold' },
                anchor: 'end',
                align: 'top',
                formatter: (value) => value
            }
        },
        scales: {
            x: {
                grid: { display: true, drawBorder: true },
                ticks: { autoSkip: false, maxRotation: 45, minRotation: 45 }
            },
            y: {
                beginAtZero: true,
                grid: { display: true }
            }
        }
    },
    plugins: [ChartDataLabels]
});
</script>
)";

    // -------------------------------------------------------
    // SIG팀 리포트
    html << R"(
<hr/>
<h1>🔍 Malware Scan Report</h1>
<h2>• Malware Scan Detection Overview</h2>
<canvas id="malwareScanDonutChart" width="400" height="400"></canvas>

<h2>• Scan Details</h2>
<table>
    <thead>
        <tr>
            <th>Date</th>
            <th>ID</th>
            <th>File Path</th>
            <th>File Name</th>
            <th>Reason</th>
            <th>Malware Name / Rule</th>
            <th>Quarantine Success Status</th>
        </tr>
    </thead>
    <tbody>
)";

    auto& scanStorage = DBManager::GetInstance().GetScanReportStorage();
    auto scanReports = scanStorage.get_all<ScanReport>(
        sqlite_orm::where(sqlite_orm::between(&ScanReport::date, mStartTime, mEndTime))
    );

    int detectedCount = 0;
    int notDetectedCount = 0;

    if (!scanReports.empty())
    {
        for (const auto& report : scanReports)
        {
            if (report.detected)
            {
                detectedCount++;
            }
            else
            {
                notDetectedCount++;
            }
        }
    }
    
    // QuarantineDB에서 데이터를 가져와 Scan Details 표를 채웁니다.
    auto& quarantineStorage = DBManager::GetInstance().GetQuarantineStorage();

    std::string convertedStartTimeForQuarantine = convertToQuarantineDateFormat(mStartTime);
    std::string convertedEndTimeForQuarantine = convertToQuarantineDateFormat(mEndTime);

    auto quarantineEntries = quarantineStorage.get_all<QuarantineMetadata>(
        sqlite_orm::where(
            sqlite_orm::between(
                &QuarantineMetadata::QuarantineDate,
                convertedStartTimeForQuarantine,
                convertedEndTimeForQuarantine
            )
        )
    );

    if (quarantineEntries.empty())
    {
        html << R"(<tr>
            <td colspan="7" style="text-align: center; font-style: italic;">
            No quarantined files found during this period.
            </td>
        </tr>
    </tbody>
</table>
)";
    }
    else
    {
        int id_counter = 1; // ID 순번을 위한 카운터
        for (const auto& entry : quarantineEntries)
        {
            html << "<tr>";
            html << "<td>" << formatQuarantineDateForDisplay(entry.QuarantineDate) << "</td>"; // Date
            html << "<td>" << id_counter++ << "</td>"; // ID 순번 표시
            html << "<td>" << entry.OriginalPath << "</td>";
            html << "<td>" << getFileNameFromPath(entry.OriginalPath) << "</td>"; // OriginalPath에서 파일 이름 추출
            html << "<td>" << generalizeReason(entry.QuarantineReason) << "</td>"; // generalizeReason 사용
            html << "<td>" << entry.MalwareNameOrRule << "</td>"; // Malware Name / Rule
            html << "<td>" << "Yes" << "</td>"; // QuarantineMetadata에 있으면 성공으로 간주
            html << "</tr>\n";
        }
        html << R"(</tbody>
</table>
)";
    }

    // SIG 차트 
    html << R"(
<script>
const scanCtx = document.getElementById('malwareScanDonutChart').getContext('2d');
new Chart(scanCtx, {
    type: 'doughnut',
    data: {
        labels: ['Detected', 'Not Detected'],
        datasets: [{
            data: [)" << detectedCount << ", " << notDetectedCount << R"(],
            backgroundColor: [
                'rgba(255, 99, 132, 0.6)', // Detected (Red)
                'rgba(75, 192, 192, 0.6)'  // Not Detected (Cyan)
            ]
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

    // --------------------------------------------------
    // LOG팀 리포트
    html << R"(
<hr/>
<h1>📜Malicious Behavior Report</h1>
)";

    if (!events.empty())
    {
        html << R"(
<h2>• Detected Malicious Behavior Types Overview</h2>
<canvas id="typeDonutChart" width="400" height="400"></canvas>
)";
    }

    html << R"(
<h2>• Detected Malicious Behavior Details</h2>
<table id="LogDetailTable">
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
        int id = 0;

        for (const auto &e : events)
        {
            typeCounts[e.type]++;
            id++;
            html << "<tr>";
            html << "<td><a href=\"#raw-" << id << "\">" << id << "</a></td>";
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
            html << "<tr id=\"raw-" << id << "\">";
            html << "<td>" << id++ << "</td>";
            html << "<td>" << e.rawLine << "</td>";
            html << "</tr>\n";
        }

        html << R"(</tbody>
</table>
)";

        std::string labelsStr, dataStr, colorStr;
        bool firstLogChartItem = true;
        size_t colorIdx = 0;

        for (const auto &pair : typeCounts)
        {
            if (!firstLogChartItem)
            {
                labelsStr += ", ";
                dataStr += ", ";
                colorStr += ", ";
            }
            labelsStr += "\"" + pair.first + "\"";
            dataStr += std::to_string(pair.second);
            colorStr += "\"" + ReportService::generateColor(colorIdx) + "\"";
            colorIdx++;
            firstLogChartItem = false;
        }

        // LOG 차트 스
        html << R"(
<script>
function highlightRow(type) {
    console.log("highlightRow called with type:", type);

    document.querySelectorAll('table tbody tr').forEach(tr => {
        tr.classList.remove('highlight');
    });

    document.querySelectorAll('#LogDetailTable tbody tr').forEach(tr => {
        const td = tr.querySelector('td:nth-child(2)');
        if (td) {
            const cellText = td.textContent.trim();
            if (cellText === type) {
                tr.classList.add('highlight');
                tr.scrollIntoView({ behavior: 'smooth', block: 'center' });
            }
        }
    });
}
const ctx = document.getElementById('typeDonutChart').getContext('2d');
const logTypeChart = new Chart(ctx, {
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
        },
        onClick: (evt) => {
            const points = logTypeChart.getElementsAtEventForMode(evt, 'nearest', { intersect: true }, true);
            if (points.length > 0) {
                const index = points[0].index;
                const label = logTypeChart.data.labels[index];
                console.log("Clicked label:", label);
                highlightRow(label);
            }
        }
    },
    plugins: [ChartDataLabels]
});
</script>
)";
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