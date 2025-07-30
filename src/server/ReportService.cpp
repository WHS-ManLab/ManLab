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
#include <iomanip> // put_time
#include <ctime> // std::tm
#include <algorithm> // std::min_element, std::abs

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

// "YYYYMMDD_HHMMSS" -> "YYYY-MM-DD HH:MM:SS" 형식으로 변환 (std::tm 파싱용)
std::string convertQuarantineDateFormatToStdFormat(const std::string& quarantineDate)
{
    if (quarantineDate.length() != 15) return quarantineDate;
    
    std::string year = quarantineDate.substr(0, 4);
    std::string month = quarantineDate.substr(4, 2);
    std::string day = quarantineDate.substr(6, 2);
    std::string hour = quarantineDate.substr(9, 2);
    std::string minute = quarantineDate.substr(11, 2);
    std::string second = quarantineDate.substr(13, 2);

    return year + "-" + month + "-" + day + " " + hour + ":" + minute + ":" + second;
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

// string (YYYY-MM-DD HH:MM:SS) to time_point
system_clock::time_point parseDateTime(const std::string& dateTimeStr) {
    std::tm t = {};
    std::istringstream ss(dateTimeStr);
    ss >> std::get_time(&t, "%Y-%m-%d %H:%M:%S");
    return system_clock::from_time_t(std::mktime(&t));
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
<h1>Malicious Behavior Report</h1>
<p style="text-align: right;">Report period: )";

    html << mStartTime << " ~ " << mEndTime  << "</p>\n";

    if (!events.empty())
    {
        html << R"(
<h2>Detected Malicious Behavior Types Overview</h2>)";
        html << R"(<canvas id="typeDonutChart" width="400" height="400"></canvas>)";
    }

    html << R"(
<h2>Detected Malicious Behavior Details</h2>
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
<h1>🔍 Malware Scan Report</h1>
<p>Scan records from )" << mStartTime << " to " << mEndTime << "</p>\n";

    auto& scanStorage = DBManager::GetInstance().GetScanReportStorage();
    auto scanReports = scanStorage.get_all<ScanReport>
    (
        sqlite_orm::where(sqlite_orm::between(&ScanReport::date, mStartTime, mEndTime))
    );

    int detectedCount = 0;
    int notDetectedCount = 0;
    int totalScans = scanReports.size(); // 총 스캔 횟수

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
    
    // QuarantineDB에서 데이터를 가져와 Scan Details 표 및 막대 그래프를 위한 데이터로 사용
    std::string convertedStartTimeForQuarantine = convertToQuarantineDateFormat(mStartTime);
    std::string convertedEndTimeForQuarantine = convertToQuarantineDateFormat(mEndTime);

    auto quarantineEntries = DBManager::GetInstance().GetQuarantineStorage().get_all<QuarantineMetadata>
    (
        sqlite_orm::where(
            sqlite_orm::between(
                &QuarantineMetadata::QuarantineDate, 
                                    convertedStartTimeForQuarantine, 
                                    convertedEndTimeForQuarantine
                                )
                        )
    );

    // Scan ID 별로 그룹화된 격리된 파일 데이터를 저장할 맵
    std::map<int, std::vector<QuarantineMetadata>> quarantinedFilesGroupedByScan;
    // Hash/YARA 탐지 갯수 집계 맵
    std::map<std::string, int> hashYaraCounts; 
    // 시간대별 탐지 결과 집계를 위한 맵
    std::map<int, std::map<std::string, int>> hourlyDetectionCounts; // hour_slot -> reason -> count

    // QuarantineMetadata를 시간 순으로 정렬하여 동일 스캔 ID 그룹화 준비
    std::vector<QuarantineMetadata> sortedQuarantineEntries = quarantineEntries;
    std::sort(sortedQuarantineEntries.begin(), sortedQuarantineEntries.end(), [](const QuarantineMetadata& a, const QuarantineMetadata& b) {
        return a.QuarantineDate < b.QuarantineDate;
    });

    int current_scan_id = 1; // Scan ID는 1부터 순차적으로 시작
    system_clock::time_point last_entry_time;

    // 탐지 기록이 있는 경우에만 ID 부여
    if (!sortedQuarantineEntries.empty()) {
        last_entry_time = parseDateTime(convertQuarantineDateFormatToStdFormat(sortedQuarantineEntries[0].QuarantineDate));
        quarantinedFilesGroupedByScan[current_scan_id].push_back(sortedQuarantineEntries[0]);
        std::string first_reason = generalizeReason(sortedQuarantineEntries[0].QuarantineReason);
        std::string first_hour_str = sortedQuarantineEntries[0].QuarantineDate.substr(9, 2);
        int first_hour = std::stoi(first_hour_str);
        int first_hour_slot = (first_hour / 3) * 3;
        hourlyDetectionCounts[first_hour_slot][first_reason]++;
        hashYaraCounts[first_reason]++;

        for (size_t i = 1; i < sortedQuarantineEntries.size(); ++i) {
            const auto& entry = sortedQuarantineEntries[i];
            system_clock::time_point entry_time = parseDateTime(convertQuarantineDateFormatToStdFormat(entry.QuarantineDate));
            
            long long diff_sec = std::abs(duration_cast<seconds>(entry_time - last_entry_time).count());
            
            if (diff_sec > 10) { // 10초 초과 시 새로운 스캔으로 간주
                current_scan_id++;
            }
            quarantinedFilesGroupedByScan[current_scan_id].push_back(entry);
            last_entry_time = entry_time; // 마지막 기록 시간 업데이트

            // 데이터 집계
            std::string reason = generalizeReason(entry.QuarantineReason);
            std::string hour_str = entry.QuarantineDate.substr(9, 2);
            int hour = std::stoi(hour_str);
            int hour_slot = (hour / 3) * 3;
            hourlyDetectionCounts[hour_slot][reason]++;
            hashYaraCounts[reason]++;
        }
    }

    html << R"(<style>
        .chart-row {
            display: flex;
            justify-content: space-around;
            flex-wrap: wrap;
            gap: 30px; /* 각 차트 박스 사이의 간격 */
            margin-bottom: 40px;
        }
        .chart-box {
            flex: 1 1 30%; /* 3개 항목이 한 줄에 */
            max-width: 400px; /* 캔버스 크기(400px)에 맞춤 */
            box-sizing: border-box; /* 패딩 포함한 크기 계산 */
            padding: 10px; /* 내부 여백 */
            text-align: center;
        }
        /* 미디어 쿼리: 3개 차트가 나란히 들어가지 않을 때, 2열로 변경 */
        @media (max-width: 1300px) { 
            .chart-box {
                flex: 1 1 45%; 
            }
        }
        /* 미디어 쿼리: 더 작은 화면에서는 1열로 변경 */
        @media (max-width: 768px) {
            .chart-box {
                flex: 1 1 90%; 
            }
        }
    </style>)";
    // ---------------------------------------------------------------------------------

    html << R"(<div class="chart-row">
    <div class="chart-box">
        <canvas id="malwareScanDonutChart" width="375" height="375" style="display: block; box-sizing: border-box; height: 400px; width: 400px;"></canvas>
        <p>총 <b>)" << totalScans << R"(</b>회의 스캔이 진행되었습니다.</p>
    </div>
    <div class="chart-box">
        <canvas id="hashYaraDonutChart" width="375" height="375" style="display: block; box-sizing: border-box; height: 400px; width: 400px;"></canvas>
        <p>총 <b>)" << (hashYaraCounts["Hash"] + hashYaraCounts["YARA"]) << R"(</b>개의 악성코드가 탐지되었습니다.</p>
    </div>
    <div class="chart-box">
        <canvas id="hourlyDetectionBarChart" width="375" height="375" style="display: block; box-sizing: border-box; height: 400px; width: 400px;"></canvas>
        <p>시간대별 악성코드 탐지 현황</p>
    </div>
</div>

<script>
// (SIG Team Chart JS START)

// 도넛 차트 클릭 시 하이라이팅을 위한 함수
function highlightScanRows(type) {
    document.querySelectorAll('#ScanDetailsTable tbody tr').forEach(tr => {
        tr.classList.remove('highlight');
    });

    if (type === 'Detected') {
        document.querySelectorAll('#ScanDetailsTable tbody tr').forEach(tr => {
            if (!tr.classList.contains('no-detection-row')) {
                tr.classList.add('highlight');
            }
        });
    } else if (type === 'Not Detected') {
        const noDetectionRow = document.querySelector('#ScanDetailsTable .no-detection-row');
        if (noDetectionRow) {
            noDetectionRow.classList.add('highlight');
        }
    }
    const firstHighlightedRow = document.querySelector('#ScanDetailsTable .highlight');
    if (firstHighlightedRow) {
        firstHighlightedRow.scrollIntoView({ behavior: 'smooth', block: 'center' });
    }
}

// Hash/YARA 도넛 차트 클릭 시 하이라이팅
function highlightReasonRows(reasonType) {
    document.querySelectorAll('#ScanDetailsTable tbody tr').forEach(tr => {
        tr.classList.remove('highlight');
    });
    document.querySelectorAll('#ScanDetailsTable tbody tr').forEach(tr => {
        if (tr.dataset.reason === reasonType) {
            tr.classList.add('highlight');
        }
    });
    const firstHighlightedRow = document.querySelector('#ScanDetailsTable .highlight');
    if (firstHighlightedRow) {
        firstHighlightedRow.scrollIntoView({ behavior: 'smooth', block: 'center' });
    }
}

// 시간대별/Hash/YARA 하이라이팅
function highlightHourlyReasonRows(reasonType, hourSlot) {
    document.querySelectorAll('#ScanDetailsTable tbody tr').forEach(tr => {
        tr.classList.remove('highlight');
    });

    const targetHourSlotStr = String(hourSlot).padStart(2, '0'); 

    document.querySelectorAll('#ScanDetailsTable tbody tr').forEach(tr => {
        const trReason = tr.dataset.reason;
        const trQuarantineHour = tr.dataset.quarantineHour;

        const trHourInt = parseInt(trQuarantineHour, 10);
        const hourSlotInt = parseInt(targetHourSlotStr, 10);

        if (trReason === reasonType && 
            trHourInt >= hourSlotInt && 
            trHourInt < (hourSlotInt + 3)) { // 3시간 범위 체크 로직 유지
            tr.classList.add('highlight');
        }
    });
    const firstHighlightedRow = document.querySelector('#ScanDetailsTable .highlight');
    if (firstHighlightedRow) {
        firstHighlightedRow.scrollIntoView({ behavior: 'smooth', block: 'center' });
    }
}


const scanCtx = document.getElementById('malwareScanDonutChart').getContext('2d');
const malwareScanDonutChart = new Chart(scanCtx, {
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
            },
            title: { // HTML p 태그로 제목을 표시하므로, Chart.js 자체의 title은 비활성화
                display: false
            }
        }
        // 이 그래프에는 onClick 하이라이팅 없음 - 요청 반영
    },
    plugins: [ChartDataLabels]
});

// Hash vs YARA 탐지율 도넛 차트
const hashYaraCtx = document.getElementById('hashYaraDonutChart').getContext('2d');
new Chart(hashYaraCtx, {
    type: 'doughnut',
    data: {
        labels: ['Hash', 'YARA'],
        datasets: [{
            data: [)" << hashYaraCounts["Hash"] << ", " << hashYaraCounts["YARA"] << R"(],
            backgroundColor: [
                'rgba(255, 159, 64, 0.6)',  // Hash (Orange)
                'rgba(153, 102, 255, 0.6)'  // YARA (Purple)
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
            },
            title: { // HTML p 태그로 제목을 표시하므로, Chart.js 자체의 title은 비활성화
                display: false
            }
        },
        onClick: (evt) => {
            const points = hashYaraCtx.getElementsAtEventForMode(evt, 'nearest', { intersect: true }, true);
            if (points.length > 0) {
                const index = points[0].index;
                const label = hashYaraCtx.data.labels[index];
                highlightReasonRows(label);
            }
        }
    },
    plugins: [ChartDataLabels]
});


// 시간대별 막대 그래프 데이터
const hourlyLabels = [];
const hashData = [];
const yaraData = [];
const barChartColors = {
    'Hash': 'rgba(255, 159, 64, 0.7)', // Orange for Hash
    'YARA': 'rgba(153, 102, 255, 0.7)' // Purple for YARA
};

)";
    // 시간대별 데이터 스크립트에 삽입 (시간대 라벨 형식 변경)
    std::time_t start_time_t = system_clock::to_time_t(parseDateTime(mStartTime));
    std::tm* start_local_tm = std::localtime(&start_time_t);
    char start_date_buf[11]; 
    std::strftime(start_date_buf, sizeof(start_date_buf), "%Y-%m-%d", start_local_tm);
    std::string start_date_str = start_date_buf;

    std::time_t end_time_t = system_clock::to_time_t(parseDateTime(mEndTime));
    std::tm* end_local_tm = std::localtime(&end_time_t);
    char end_date_buf[11]; 
    std::strftime(end_date_buf, sizeof(end_date_buf), "%Y-%m-%d", end_local_tm);
    std::string end_date_str = end_date_buf;

    for (int h_idx = 0; h_idx < 8; ++h_idx) {
        int h = h_idx * 3;
        std::string label_str;
        if (h_idx == 0) { // 첫 번째 라벨 (0시)
            label_str = start_date_str + " " + std::string(std::to_string(h)).insert(0, 2 - std::to_string(h).length(), '0') + ":00";
        } else if (h_idx == 7) { // 마지막 라벨 (21시)
             label_str = end_date_str + " " + std::string(std::to_string(h)).insert(0, 2 - std::to_string(h).length(), '0') + ":00";
        } else { // 중간 라벨
            label_str = std::string(std::to_string(h)).insert(0, 2 - std::to_string(h).length(), '0') + ":00";
        }
        html << "hourlyLabels.push('" << label_str << "');\n";
        
        html << "hashData.push(" << hourlyDetectionCounts[h]["Hash"] << ");\n";
        html << "yaraData.push(" << hourlyDetectionCounts[h]["YARA"] << ");\n";
    }

html << R"(
const barCtx = document.getElementById('hourlyDetectionBarChart').getContext('2d');
const hourlyDetectionBarChart = new Chart(barCtx, { // Chart 객체 변수 추가
    type: 'bar',
    data: {
        labels: hourlyLabels,
        datasets: [
            {
                label: 'Hash Detections',
                data: hashData,
                backgroundColor: barChartColors['Hash'],
                borderColor: barChartColors['Hash'].replace('0.7', '1'),
                borderWidth: 1
            },
            {
                label: 'YARA Detections',
                data: yaraData,
                backgroundColor: barChartColors['YARA'],
                borderColor: barChartColors['YARA'].replace('0.7', '1'),
                borderWidth: 1
            }
        ]
    },
    options: {
        responsive: false,
        scales: {
            x: {
                stacked: true,
                title: {
                    display: true,
                    text: 'Time Interval'
                }
            },
            y: {
                stacked: true,
                beginAtZero: true,
                ticks: {
                    stepSize: 1 // Y축 단위를 1로 설정
                },
                title: {
                    display: true,
                    text: 'Number of Detections'
                }
            }
        },
        plugins: {
            legend: {
                position: 'bottom'
            },
            datalabels: {
                display: false
            },
            title: { // HTML p 태그로 제목을 표시하므로, Chart.js 자체의 title은 비활성화
                display: false
            }
        },
        onClick: (evt) => { // 막대 그래프 onClick 이벤트 추가
            const points = hourlyDetectionBarChart.getElementsAtEventForMode(evt, 'nearest', { intersect: true }, true);
            if (points.length > 0) {
                const datasetIndex = points[0].datasetIndex; // Hash인지 YARA인지 (0 또는 1)
                const elementIndex = points[0].index;      // 시간 슬롯 인덱스 (0~7)

                const reasonType = (datasetIndex === 0) ? 'Hash' : 'YARA'; // datasets[0]이 Hash, [1]이 YARA로 일치해야 함
                const hourSlot = elementIndex * 3; // 시간 슬롯 인덱스를 실제 시간으로 변환 (예: 0->0, 1->3, 7->21)
                
                highlightHourlyReasonRows(reasonType, hourSlot);
            }
        }
    },
    plugins: [ChartDataLabels]
});
</script>

<h2>Scan Details</h2>
<table id="ScanDetailsTable">
    <thead>
        <tr>
            <th>Scan ID</th>
            <th>Reason</th>
            <th>Malware Name / Rule</th>
            <th>File Path</th>
            <th>File Name</th>
            <th>Date</th>
            <th>Quarantine Success Status</th>
        </tr>
    </thead>
    <tbody>
)";

    // 탐지된 악성코드 기록이 없는 경우
    if (quarantineEntries.empty())
    {
        html << R"(<tr class="no-detection-row">
            <td colspan="7" style="text-align: center; font-style: italic;">
            탐지된 악성코드가 없습니다.
            </td>
        </tr>
    </tbody>
</table>
)";
    }
    else
    {
        // Scan ID 별로 그룹화된 데이터를 출력 (Scan ID 기준 오름차순 정렬)
        for (const auto& pair : quarantinedFilesGroupedByScan) {
            int scan_id = pair.first;
            const auto& entries_for_scan = pair.second;
            
            // 각 스캔 ID 그룹 내에서는 QuarantineDate를 기준으로 정렬
            std::vector<QuarantineMetadata> sorted_entries_in_group = entries_for_scan;
            std::sort(sorted_entries_in_group.begin(), sorted_entries_in_group.end(), [](const QuarantineMetadata& a, const QuarantineMetadata& b) {
                return a.QuarantineDate < b.QuarantineDate;
            });

            for (const auto& entry : sorted_entries_in_group) {
                std::string quarantineHourStr = entry.QuarantineDate.substr(9, 2);
                html << "<tr data-scan-id=\"" << scan_id << "\" data-reason=\"" << generalizeReason(entry.QuarantineReason) << "\" data-quarantine-hour=\"" << quarantineHourStr << "\">"; 
                html << "<td>" << scan_id << "</td>"; // Scan ID 표시
                html << "<td>" << generalizeReason(entry.QuarantineReason) << "</td>"; // generalizeReason 사용
                html << "<td>" << entry.MalwareNameOrRule << "</td>"; // Malware Name / Rule
                html << "<td>" << entry.OriginalPath << "</td>";
                html << "<td>" << getFileNameFromPath(entry.OriginalPath) << "</td>"; // OriginalPath에서 파일 이름 추출
                html << "<td>" << formatQuarantineDateForDisplay(entry.QuarantineDate) << "</td>"; // Date
                html << "<td>" << "Yes" << "</td>"; // QuarantineMetadata에 있으면 성공으로 간주
                html << "</tr>\n";
            }
        }
        html << R"(</tbody>
</table>
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