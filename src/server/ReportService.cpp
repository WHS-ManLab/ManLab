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
#include <algorithm>
#include <iomanip>    // std::put_time


using namespace std::chrono;
using namespace manlab::utils;

namespace 
{
    // "YYYYMMDD_HHMMSS" -> "YYYY-MM-DD HH:MM:SS" 형식으로 변환 (std::get_time 파싱용)
    std::string convertQuarantineDateFormatToStdFormat(const std::string& quarantineDate)
    {
        if (quarantineDate.length() != 15 || quarantineDate[8] != '_')
        {
            return quarantineDate;
        }
        return quarantineDate.substr(0, 4) + "-" + quarantineDate.substr(4, 2) + "-" + quarantineDate.substr(6, 2) + " " +
               quarantineDate.substr(9, 2) + ":" + quarantineDate.substr(11, 2) + ":" + quarantineDate.substr(13, 2);
    }

    // std::string 형태의 날짜/시간을 std::chrono::system_clock::time_point로 파싱
    std::chrono::system_clock::time_point parseDateTime(const std::string& datetimeStr) 
    {
        std::tm tm = {};
        std::istringstream ss(datetimeStr);
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
        if (ss.fail()) 
        {
            throw std::runtime_error("Failed to parse datetime string: " + datetimeStr);
        }
        tm.tm_isdst = -1;
        return std::chrono::system_clock::from_time_t(std::mktime(&tm));
    }
}

//3시간 단위로 시간 분리
std::string get3HourBucketLabel(const std::string& datetimeStr)
{
    std::tm tm = {};
    std::istringstream ss(datetimeStr);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");

    if (ss.fail())
    {
        return "";  // 파싱 실패 시 빈 문자열 반환
    }

    int bucketStart = (tm.tm_hour / 3) * 3;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%s %02d:00",
                  datetimeStr.substr(0,10).c_str(),  // 날짜
                  bucketStart);                     // 3시간 단위로 내림
    return std::string(buf);
}

//Time 차트용 함수 (x축 라벨과 y축 라벨 자동으로 채워줌)
 void generate3HourLabels(const std::string& startTime, const std::string& endTime,
                         std::vector<std::string>& timeLabels, std::vector<int>& counts,
                         const std::map<std::string, int>& bucketMap)
{
    timeLabels.clear();
    counts.clear();

    std::tm t0 = {};
    std::tm t1 = {};
    std::istringstream(startTime) >> std::get_time(&t0, "%Y-%m-%d %H:%M:%S");
    std::istringstream(endTime)   >> std::get_time(&t1, "%Y-%m-%d %H:%M:%S");
    auto start = std::mktime(&t0);
    auto end   = std::mktime(&t1);

    t0 = *std::localtime(&start);
    t0.tm_hour = (t0.tm_hour / 3) * 3;
    t0.tm_min = t0.tm_sec = 0;
    start = std::mktime(&t0);

    for (auto tt = start; tt <= end; tt += 3 * 3600)
    {
        std::tm tb = *std::localtime(&tt);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:00", &tb);
        std::string lbl(buf);
        timeLabels.push_back(lbl);
        counts.push_back(bucketMap.count(lbl) ? bucketMap.at(lbl) : 0);
    }
}

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
        hr {
            height : 50px;
            border : 0;
        }
        .chart-row {
            display: flex;
            justify-content: space-around;
            flex-wrap: wrap;
            gap: 30px;
            margin-bottom: 40px;
        }
        .chart-box {
            flex: 1 1 30%;
            max-width: 400px;
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
<div style="display: flex; justify-content: space-between; gap: 20px; flex-wrap: wrap;">
    <!-- 확장자별 차트 + 범례 -->
    <div style="display: flex; align-items: center;">
        <canvas id="manualScanExtChart" width="400" height="400"></canvas>
        <div id="manualScanExtLegend" style="margin-left: 20px;"></div>
    </div>

    <!-- 변경 사유별 차트 -->
    <div>
        <canvas id="manualScanReasonChart" width="400" height="400"></canvas>
    </div>

    <!-- 시간대별 차트 -->
    <div>
        <canvas id="manualScanTimeChart" width="400" height="400"></canvas>
    </div>
</div>

<table id = "manualScanTable">
    <thead>
        <tr>
            <th>ID</th>
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

    // [추가] 추가된 시간대별 통계
    std::map<std::string, int> manualTimeBuckets;
    std::vector<std::string> allTimeLabels;
    std::vector<int> allTimeCounts;

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
        int rowId = 1;

        for (const auto& record : modifiedRecords) 
        {
            html << "<tr>";
            html << "<td>" << rowId++ << "</td>";
            html << "<td>" << record.path << "</td>";

            auto baselineEntryOptional = baselineStorage.get_optional<BaselineEntry>(record.path);

            bool changed_md5 = false, changed_perm = false, changed_uid = false;
            bool changed_gid = false, changed_ctime = false, changed_mtime = false, changed_size = false;

            if (baselineEntryOptional) 
            {
                const auto& baseline = *baselineEntryOptional;
                changed_md5   = record.current_md5 != baseline.md5;
                changed_perm  = record.current_permission != baseline.permission;
                changed_uid   = record.current_uid != baseline.uid;
                changed_gid   = record.current_gid != baseline.gid;
                changed_ctime = record.current_ctime != baseline.ctime;
                changed_mtime = record.current_mtime != baseline.mtime;
                changed_size  = record.current_size != baseline.size;

                // 수치 카운트
                if (changed_md5)   manualReasonCounts["해시값 변경"]++;
                if (changed_perm)  manualReasonCounts["권한 변경"]++;
                if (changed_uid)   manualReasonCounts["UID 변경"]++;
                if (changed_gid)   manualReasonCounts["GID 변경"]++;
                if (changed_ctime) manualReasonCounts["CTime 변경"]++;
                if (changed_mtime) manualReasonCounts["MTime 변경"]++;
                if (changed_size)  manualReasonCounts["크기 변경"]++;
            }

            // 변경 여부에 따라 data-changed 속성 부여
            html << "<td data-changed=\"" << (changed_md5   ? "true" : "false") << "\">" << record.current_md5 << "</td>";
            html << "<td data-changed=\"" << (changed_perm  ? "true" : "false") << "\">" << record.current_permission << "</td>";
            html << "<td data-changed=\"" << (changed_uid   ? "true" : "false") << "\">" << record.current_uid << "</td>";
            html << "<td data-changed=\"" << (changed_gid   ? "true" : "false") << "\">" << record.current_gid << "</td>";
            html << "<td data-changed=\"" << (changed_ctime ? "true" : "false") << "\">" << record.current_ctime << "</td>";
            html << "<td data-changed=\"" << (changed_mtime ? "true" : "false") << "\">" << record.current_mtime << "</td>";
            html << "<td data-changed=\"" << (changed_size  ? "true" : "false") << "\">" << record.current_size << "</td>";
            html << "</tr>\n";

            // 확장자 카운트
            std::string ext = record.path.substr(record.path.find_last_of('.') + 1);
            if (ext.empty()) ext = "unknown";
            manualTypeCounts[ext]++;
             //[추가] 3시간 단위 시간 라벨 추출
            std::string timeLabel = get3HourBucketLabel(record.current_mtime);
            if (!timeLabel.empty())
            {
                manualTimeBuckets[timeLabel]++;
            }
        }

    // [추가]최종 라벨/카운트 배열 생성
    generate3HourLabels(mStartTime, mEndTime, allTimeLabels, allTimeCounts, manualTimeBuckets);
    }

    html << R"(</tbody></table>)";

    

    html << R"(
<h2>• Real-time Monitoring Events</h2>
<div style="display: flex; justify-content: center;">
    <canvas id="realtimeChart" style="margin-right: 10px;" width="400" height="400"></canvas>
    <canvas id="realtimeTimeChart" width="400" height="400"></canvas>
</div>
<table id = "realtimeTable">
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
            <td colspan="5" style="text-align: center; font-style: italic;">
            No real-time monitoring events detected during this period.
            </td>
        </tr>
)";
    } else {
        int rowId = 1;
        for (const auto& record : realTimeRecords) {
            std::string trimmedTimestamp = record.timestamp;
            std::size_t dotPos = trimmedTimestamp.find('.');
            if (dotPos != std::string::npos) {
                trimmedTimestamp = trimmedTimestamp.substr(0, dotPos);
            }

            html << "<tr><td>" << rowId++ << "</td><td>" << record.path
                << "</td><td>" << record.eventType << "</td><td>" << record.newName
                << "</td><td>" << trimmedTimestamp << "</td></tr>";

            eventTypeCounts[record.eventType]++;
        }
    }
    html << R"(</tbody></table>)";

    //시간 차트 추가
    std::map<std::string, int> realTimeTimeBuckets;
    for (const auto& record : realTimeRecords)
    {
        std::string timeLabel = get3HourBucketLabel(record.timestamp);
        if (!timeLabel.empty())
        {
            realTimeTimeBuckets[timeLabel]++;
        }
    }

    std::vector<std::string> eventTimeLabels;
    std::vector<int> eventTimeCounts;
    generate3HourLabels(mStartTime, mEndTime, eventTimeLabels, eventTimeCounts, realTimeTimeBuckets);

    // FIM 차트 스크립트
    html << R"(
<script>

function highlightManualRowByExt(ext) {
    document.querySelectorAll('#manualScanTable tbody tr').forEach(tr => {
        tr.classList.remove('highlight');
        const pathCell = tr.querySelector('td:nth-child(2)');
        if (pathCell && pathCell.textContent.trim().endsWith('.' + ext)) {
            tr.classList.add('highlight');
        }
    });
}

function highlightManualRowByReason(reason) {
    const reasonColMap = {
        "해시값 변경": 3,
        "권한 변경": 4,
        "UID 변경": 5,
        "GID 변경": 6,
        "CTime 변경": 7,
        "MTime 변경": 8,
        "크기 변경": 9
    };
    const colIdx = reasonColMap[reason];

    document.querySelectorAll('#manualScanTable tbody tr').forEach(tr => {
        tr.classList.remove('highlight');
        const td = tr.querySelector(`td:nth-child(${colIdx})`);
        if (td && td.dataset.changed === "true") {
            tr.classList.add('highlight');
        }
    });
}

function highlightManualRowByTime(timeLabel) {
    const startTime = new Date(timeLabel.replace(" ", "T") + ":00");
    const endTime = new Date(startTime.getTime() + 3 * 60 * 60 * 1000);

    document.querySelectorAll('#manualScanTable tbody tr').forEach(tr => {
        tr.classList.remove('highlight');
        const mtimeCell = tr.querySelector('td:nth-child(8)');
        if (mtimeCell) {
            const mtime = new Date(mtimeCell.textContent.trim().replace(" ", "T"));
            if (mtime >= startTime && mtime < endTime) {
                tr.classList.add('highlight');
            }
        }
    });
}

function highlightRealtimeRowByType(type) {
    const target = type.trim().toLowerCase();

    document.querySelectorAll('#realtimeTable tbody tr').forEach(tr => {
        tr.classList.remove('highlight');
        const td = tr.querySelector('td:nth-child(3)');
        if (td && td.textContent.trim().toLowerCase() === target) {
            tr.classList.add('highlight');
        }
    });

    const first = document.querySelector('#realtimeTable tbody tr.highlight');
    if (first) first.scrollIntoView({ behavior: 'smooth', block: 'center' });
}

function highlightRealtimeRowByTime(label) {
    const startTime = new Date(label.replace(" ", "T") + ":00");
    const endTime = new Date(startTime.getTime() + 3 * 60 * 60 * 1000);

    document.querySelectorAll('#realtimeTable tbody tr').forEach(tr => {
        tr.classList.remove('highlight');
        const td = tr.querySelector('td:nth-child(5)');
        if (td) {
            const eventTime = new Date(td.textContent.trim().replace(" ", "T"));
            if (eventTime >= startTime && eventTime < endTime) {
                tr.classList.add('highlight');
            }
        }
    });

    const first = document.querySelector('#realtimeTable tbody tr.highlight');
    if (first) first.scrollIntoView({ behavior: 'smooth', block: 'center' });
}

// File Extension 도넛
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
        onClick: (evt) => {
            const points = extChart.getElementsAtEventForMode(evt, 'nearest', { intersect: true }, true);
            if (points.length > 0) {
                const label = extChart.data.labels[points[0].index];
                highlightManualRowByExt(label);
            }
        },
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
                font: {size: 18, weight: 'bold' }
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

//File Change Reasons 막대 그래프
const reasonCtx = document.getElementById('manualScanReasonChart').getContext('2d');
const reasonChart = new Chart(reasonCtx, {
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
        onClick: (evt) => {
            const points = reasonChart.getElementsAtEventForMode(evt, 'nearest', { intersect: true }, true);
            if (points.length > 0) {
                const label = reasonChart.data.labels[points[0].index];
                highlightManualRowByReason(label);
            }
        },

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
                font: {size: 18, weight: 'bold' },
                padding: { bottom : 20}
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

//Modification By Time
const timeCtx = document.getElementById('manualScanTimeChart').getContext('2d');
    const timeChart = new Chart(timeCtx, {
        type: 'bar',
        data: {
            labels: [)";

    // 수정된 파일 갯수
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
            Color: [)";
    for (size_t i = 0; i < allTimeCounts.size(); ++i) {
        if (i) html << ", ";
        html << "\"" << ReportService::generateColor(i) << "\"";
    }
    html << R"(]
        }]
    },
    options: {
        responsive: false,
        onClick: (evt) => {
            const points = timeChart.getElementsAtEventForMode(evt, 'nearest', { intersect: true }, true);
            if (points.length > 0) {
                const label = timeChart.data.labels[points[0].index];
                highlightManualRowByTime(label);
            }
        },
        plugins: {
            legend: { display: false },
            title: {
                display: true,
                text: ' Modification By Time',
                font: {size: 18, weight: 'bold' }
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
                ticks: { autoSkip: false, maxRotation: 45, minRotation: 45 },
                title : {
                    display : false,
                    text : 'Time Interval (3hours)',
                    font: {
                        size : 13,
                        weight: 'bold'
                    },
                    padding: {top : 10},
                }
            },
            y: {
                beginAtZero: true,
                grid: { display: true },
                title: {
                    display: false,
                    text: 'Number of Events',
                    font: {
                        size: 13,
                        weight: 'bold'
                    },
                    padding: { bottom: 10 },
                }
            }
        }
    },
    plugins: [ChartDataLabels]
});

//Events By Type 막대 그래프
const rtCtx = document.getElementById('realtimeChart').getContext('2d');
const rtChart = new Chart(rtCtx, {
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
        onClick: (evt) => {
            const points = rtChart.getElementsAtEventForMode(evt, 'nearest', { intersect: true }, true);
            if (points.length > 0) {
                const label = rtChart.data.labels[points[0].index];
                highlightRealtimeRowByType(label);
            }
        },
        plugins: {
            legend: { display: false },
            title: {
                display: true,
                text: ' Events By Type',
                font: {size: 18, weight: 'bold' }
            },
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

//Events by Time 시간 막대 그래프
const rtTimeCtx = document.getElementById('realtimeTimeChart').getContext('2d');
const rtTimeChart = new Chart(rtTimeCtx, {
    type: 'bar',
    data: {
        labels: [)";
for (size_t i = 0; i < eventTimeLabels.size(); ++i) {
    if (i) html << ", ";
    html << "\"" << eventTimeLabels[i] << "\"";
}
html << R"(],
        datasets: [{
            label: 'Events By Time',
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
        onClick: (evt) => {
            const points = rtTimeChart.getElementsAtEventForMode(evt, 'nearest', { intersect: true }, true);
            if (points.length > 0) {
                const label = rtTimeChart.data.labels[points[0].index];
                highlightRealtimeRowByTime(label);
            }
        },
        plugins: {
            legend: { display: false },
            title: {
                display: true,
                text: 'Events by Time',
                font: {size: 18, weight: 'bold' }
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
                ticks: { autoSkip: false, maxRotation: 45, minRotation: 45 },
                title : {
                    display : false,
                    text : 'Time Interval (3hours)',
                    font: {
                        size : 13,
                        weight: 'bold'
                    },
                    padding: {top : 10},
                }
            },
            y: {
                beginAtZero: true,
                grid: { display: true },
                title: {
                    display: false,
                    text: 'Number of Events',
                    font: {
                        size: 13,
                        weight: 'bold'
                    },
                    padding: { bottom: 10 },
                }
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
<hr/>
<h1>🔍 Malware Scan Report</h1>
<h2>• Malware Scan Detection Overview</h2>
)";

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
    // 시간대별 탐지 결과 집계를 위한 임시 맵 (generate3HourLabels에 사용하기 위함)
    std::map<std::string, std::map<std::string, int>> tempHourlyReasonCounts; 

    // QuarantineMetadata를 시간 순으로 정렬하여 동일 스캔 ID 그룹화 준비
    std::vector<QuarantineMetadata> sortedQuarantineEntries = quarantineEntries;
    std::sort(sortedQuarantineEntries.begin(), sortedQuarantineEntries.end(), [](const QuarantineMetadata& a, const QuarantineMetadata& b) {
        return a.QuarantineDate < b.QuarantineDate;
    });

    int current_scan_id = 1; // Scan ID는 1부터 순차적으로 시작
    system_clock::time_point last_entry_time;

    // 탐지 기록이 있는 경우에만 ID 부여
    if (!sortedQuarantineEntries.empty()) {
        // 첫 번째 항목 처리
        last_entry_time = parseDateTime(convertQuarantineDateFormatToStdFormat(sortedQuarantineEntries[0].QuarantineDate));
        quarantinedFilesGroupedByScan[current_scan_id].push_back(sortedQuarantineEntries[0]);
        std::string first_reason = generalizeReason(sortedQuarantineEntries[0].QuarantineReason);
        std::string first_bucket_label = get3HourBucketLabel(convertQuarantineDateFormatToStdFormat(sortedQuarantineEntries[0].QuarantineDate));
        tempHourlyReasonCounts[first_bucket_label][first_reason]++;
        hashYaraCounts[first_reason]++;

        // 나머지 항목 처리
        for (size_t i = 1; i < sortedQuarantineEntries.size(); ++i) {
            const auto& entry = sortedQuarantineEntries[i];
            system_clock::time_point entry_time = parseDateTime(convertQuarantineDateFormatToStdFormat(entry.QuarantineDate));
            
            long long diff_sec = std::abs(duration_cast<seconds>(entry_time - last_entry_time).count());
            
            if (diff_sec > 10) { // 10초 초과 시 새로운 스캔으로 간주 (기준을 유동적으로 조정할 수 있습니다)
                current_scan_id++;
            }
            quarantinedFilesGroupedByScan[current_scan_id].push_back(entry);
            last_entry_time = entry_time; // 마지막 기록 시간 업데이트

            // 데이터 집계
            std::string reason = generalizeReason(entry.QuarantineReason);
            std::string bucket_label = get3HourBucketLabel(convertQuarantineDateFormatToStdFormat(entry.QuarantineDate));
            tempHourlyReasonCounts[bucket_label][reason]++;
            hashYaraCounts[reason]++;
        }
    }
    
    // generate3HourLabels 함수를 사용하여 모든 시간대 라벨과 각 시간대의 Hash, YARA 카운트 추출
    std::vector<std::string> sigHourlyLabels;
    std::vector<int> dummySigCounts; // generate3HourLabels 호출을 위한 더미
    std::map<std::string, int> dummyBucketMapForLabels; // 시간 라벨만 얻기 위한 더미 맵

    generate3HourLabels(mStartTime, mEndTime, sigHourlyLabels, dummySigCounts, dummyBucketMapForLabels);

    // 이제 sigHourlyLabels를 기준으로 실제 hashData와 yaraData를 채웁니다.
    std::string sigHourlyLabelsJs, hashDataJs, yaraDataJs;
    bool firstHourlyItem = true;

    for (const auto& label : sigHourlyLabels) {
        if (!firstHourlyItem) {
            sigHourlyLabelsJs += ", ";
            hashDataJs += ", ";
            yaraDataJs += ", ";
        }
        sigHourlyLabelsJs += "\"" + label + "\"";
        hashDataJs += std::to_string(tempHourlyReasonCounts[label]["Hash"]);
        yaraDataJs += std::to_string(tempHourlyReasonCounts[label]["YARA"]);
        firstHourlyItem = false;
    }

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
function highlightHourlyReasonRows(reasonType, hourSlotLabel) {
    document.querySelectorAll('#ScanDetailsTable tbody tr').forEach(tr => {
        tr.classList.remove('highlight');
    });

    // 'YYYY-MM-DD HH:00' 형태의 시간 라벨을 Date 객체로 변환
    const startTimeLabel = hourSlotLabel;
    const startTime = new Date(startTimeLabel.replace(" ", "T") + ":00"); // 예: "2025-07-31T09:00:00"
    const endTime = new Date(startTime.getTime() + 3 * 60 * 60 * 1000); // 3시간 후

    document.querySelectorAll('#ScanDetailsTable tbody tr').forEach(tr => {
        const trReason = tr.dataset.reason;
        const trQuarantineDateStr = tr.dataset.quarantineDate; // YYYYMMDD_HHMMSS
        
        // 표의 QuarantineDate를 Date 객체로 변환
        if (!trQuarantineDateStr) return; // 데이터가 없으면 스킵

        const year = trQuarantineDateStr.substring(0, 4);
        const month = trQuarantineDateStr.substring(4, 6);
        const day = trQuarantineDateStr.substring(6, 8);
        const hour = trQuarantineDateStr.substring(9, 11);
        const minute = trQuarantineDateStr.substring(11, 13);
        const second = trQuarantineDateStr.substring(13, 15);
        const entryDateTime = new Date(`${year}-${month}-${day}T${hour}:${minute}:${second}`);
        
        if (trReason === reasonType && entryDateTime >= startTime && entryDateTime < endTime) {
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
            title: { 
                display: true,
                text: "Malware Scan Result",
                font: { size: 18, weight: 'bold' }
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
            title: { 
                display: true,
                text: "Malware Detection By Type",
                font: { size: 18, weight: 'bold' }
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


// 시간대별 막대 그래프 데이터 (generate3HourLabels 로직 반영)
const hourlyLabels = [)" << sigHourlyLabelsJs << R"(]; // 직접 C++에서 생성한 라벨 사용 (요청 5 반영)
const hashData = [)" << hashDataJs << R"(];
const yaraData = [)" << yaraDataJs << R"(];

const barChartColors = {
    'Hash': 'rgba(255, 159, 64, 0.7)', // Orange for Hash
    'YARA': 'rgba(153, 102, 255, 0.7)' // Purple for YARA
};

const barCtx = document.getElementById('hourlyDetectionBarChart').getContext('2d');
const hourlyDetectionBarChart = new Chart(barCtx, { 
    type: 'bar',
    data: {
        labels: hourlyLabels,
        datasets: [
            {
                label: 'Hash',
                data: hashData,
                backgroundColor: barChartColors['Hash'],
                borderColor: barChartColors['Hash'].replace('0.7', '1'),
                borderWidth: 1
            },
            {
                label: 'YARA',
                data: yaraData,
                backgroundColor: barChartColors['YARA'],
                borderColor: barChartColors['YARA'].replace('0.7', '1'),
                borderWidth: 1
            }
        ]
    },
    options: {
        responsive: false,
        scales: 
        {
            x: {
                stacked: true,
                title: 
                {
                    display: false
                }
            },
            y: 
            {
                stacked: true,
                beginAtZero: true,
                ticks: 
                {
                    stepSize: 1 
                },
                title: 
                {
                    display: false
                }
            }
        },
        plugins: 
        {
            legend: 
            {
                position: 'bottom'
            },
            datalabels: 
            {
                display: false
            },
            title: 
            { 
                display: true,
                text: "Malware Detection By Time",
                font: { size: 18, weight: 'bold' }
            }
        },
        onClick: (evt) => 
        { 
            const points = hourlyDetectionBarChart.getElementsAtEventForMode(evt, 'nearest', { intersect: true }, true);
            if (points.length > 0) 
            {
                const datasetIndex = points[0].datasetIndex; 
                const elementIndex = points[0].index;      

                const reasonType = (datasetIndex === 0) ? 'Hash' : 'YARA'; 
                const hourSlotLabel = hourlyLabels[elementIndex]; // 직접 라벨 전달
                
                highlightHourlyReasonRows(reasonType, hourSlotLabel);
            }
        }
    },
    plugins: [ChartDataLabels]
});
</script>

<h2>• Scan Details</h2>
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
        </tr></thead><tbody>
)";

    // 탐지된 악성코드 기록이 없는 경우
    if (quarantineEntries.empty())
    {
        html << R"(<tr class="no-detection-row">
            <td colspan="7" style="text-align: center; font-style: italic;">
            No quarantined files found during this period.
            </td></tr></tbody></table>)";
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
                // Table row에 data-quarantine-date 속성 추가
                html << "<tr data-scan-id=\"" << scan_id << "\" data-reason=\"" << generalizeReason(entry.QuarantineReason) << "\" data-quarantine-date=\"" << entry.QuarantineDate << "\">"; 
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
        html << R"(</tbody></table>)";
    }


    // --------------------------------------------------
    // LOG팀 리포트
    html << R"(
<hr/>
<h1>📜Malicious Behavior Report</h1>
)";

    if (!events.empty())
    {
        html << R"(
<h2>• Malicious Behavior Overview</h2>
<div class="chart-row">
    <div class="chart-box">
        <canvas id="typeDonutChart" width="400" height="400"></canvas>
    </div>
    <div class="chart-box">
        <canvas id="userBarChart" width="400" height="400"></canvas>
    </div>
    <div class="chart-box">
        <canvas id="timeHistogram" width="400" height="400"></canvas>
    </div>
</div>
)";
    }

    std::map<std::string, int> logTypeCounts;
    std::map<std::string, int> logUserCounts;
    std::map<std::string, int> logTimeBuckets;

    for (const auto &e : events)
    {
        logTypeCounts[e.type]++;
        logUserCounts[e.username]++;

        std::string timeLabel = get3HourBucketLabel(e.timestamp);
        if (!timeLabel.empty())
        {
            logTimeBuckets[timeLabel]++;
        }  
    }

    std::vector<std::string> logTimeLabels;
    std::vector<int> logTimeCounts;
    generate3HourLabels(mStartTime, mEndTime, logTimeLabels, logTimeCounts, logTimeBuckets);

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
        int id = 0;
        for (const auto &e : events)
        {
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

<h2>• Event Log Line by ID</h2>
<table>
    <thead>
        <tr>
            <th>Event ID</th>
            <th>Raw Log Line</th>
        </tr>
    </thead>
    <tbody>
)";

        id = 0;
        for (const auto &e : events)
        {
            html << "<tr id=\"raw-" << ++id << "\">";
            html << "<td>" << id << "</td>";
            html << "<td>" << e.rawLine << "</td>";
            html << "</tr>\n";
        }

        html << R"(</tbody>
</table>
)";

        std::string typeLabels, typeData, typeColorStr, userLabels, userData, userColorStr;
        bool firstLogChartItem = true;
        size_t typeColorIdx = 0, userColorIdx = 0;

        for (const auto &pair : logTypeCounts)
        {
            if (!firstLogChartItem)
            {
                typeLabels += ", ";
                typeData += ", ";
                typeColorStr += ", ";
            }
            typeLabels += "\"" + pair.first + "\"";
            typeData += std::to_string(pair.second);
            typeColorStr += "\"" + ReportService::generateColor(typeColorIdx) + "\"";
            typeColorIdx++;
            firstLogChartItem = false;
        }

        firstLogChartItem = true;
        for (const auto &pair : logUserCounts)
        {
            if (!firstLogChartItem)
            {
                userLabels += ", ";
                userData += ", ";
                userColorStr += ", ";
            }
            userLabels += "\"" + pair.first + "\"";
            userData += std::to_string(pair.second);
            userColorStr += "\"" + ReportService::generateColor(userColorIdx) + "\"";
            userColorIdx++;
            firstLogChartItem = false;
        }
        
        firstLogChartItem = true;
        std::string timeLabels, timeData;
        for (size_t i = 0; i < logTimeLabels.size(); ++i)
        {
            if (!firstLogChartItem)
            {
                timeLabels += ", ";
                timeData += ", ";
            }
            timeLabels += "\"" + logTimeLabels[i] + "\"";
            timeData += std::to_string(logTimeCounts[i]);
            firstLogChartItem = false;
        }

        // LOG 차트 스크립트
        html << R"(
<script>
function highlightRowType(type) {
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
            }
        }
    });

    const firstHighlighted = document.querySelector('#LogDetailTable tbody tr.highlight');
    if (firstHighlighted) {
        firstHighlighted.scrollIntoView({ behavior: 'smooth', block: 'center' });
    }
}

function highlightRowUser(user) {
    console.log("highlightRow called with user:", user);

    document.querySelectorAll('table tbody tr').forEach(tr => {
        tr.classList.remove('highlight');
    });

    document.querySelectorAll('#LogDetailTable tbody tr').forEach(tr => {
        const td = tr.querySelector('td:nth-child(5)');
        if (td) {
            const cellText = td.textContent.trim();
            if (cellText === user) {
                tr.classList.add('highlight');
            }
        }
    });

    const firstHighlighted = document.querySelector('#LogDetailTable tbody tr.highlight');
    if (firstHighlighted) {
        firstHighlighted.scrollIntoView({ behavior: 'smooth', block: 'center' });
    }
}

function highlightRowTime(timeLabel) {
    console.log("highlightRow called with time:", timeLabel);

    const startTime = new Date(timeLabel.replace(" ", "T") + ":00");
    const endTime = new Date(startTime.getTime() + 3 * 60 * 60 * 1000); // +3시간

    document.querySelectorAll('table tbody tr').forEach(tr => {
        tr.classList.remove('highlight');
    });

    document.querySelectorAll('#LogDetailTable tbody tr').forEach(tr => {
        const td = tr.querySelector('td:nth-child(4)');
        if (td) {
            const cellText = td.textContent.trim();
            const eventTime = new Date(cellText.replace(" ", "T"));
            if (eventTime >= startTime && eventTime < endTime) {
                tr.classList.add('highlight');
            }
        }
    });

    const firstHighlighted = document.querySelector('#LogDetailTable tbody tr.highlight');
    if (firstHighlighted) {
        firstHighlighted.scrollIntoView({ behavior: 'smooth', block: 'center' });
    }
}

const logTypeCtx = document.getElementById('typeDonutChart').getContext('2d');
const logTypeChart = new Chart(logTypeCtx, {
    type: 'doughnut',
    data: {
        labels: [)" << typeLabels << R"(],
        datasets: [{
            data: [)" << typeData << R"(],
            backgroundColor: [)" << typeColorStr << R"(]
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
                text: "Malicious Behavior By Type",
                font: { size: 18, weight: 'bold' }
            }
        },
        onClick: (evt) => {
            const points = logTypeChart.getElementsAtEventForMode(evt, 'nearest', { intersect: true }, true);
            if (points.length > 0) {
                const index = points[0].index;
                const label = logTypeChart.data.labels[index];
                console.log("Clicked label:", label);
                highlightRowType(label);
            }
        }
    },
    plugins: [ChartDataLabels]
});

const logUserCtx = document.getElementById('userBarChart').getContext('2d');
const logUserChart = new Chart(logUserCtx, {
    type: 'bar',
    data: {
            labels: [)" << userLabels << R"(],
            datasets: [{
            label: 'Event Count',
            data: [)" << userData << R"(],
            backgroundColor: 'rgba(54,162,235,0.6)'
        }]
    },
    options: {
        indexAxis: 'y',
        responsive: false, 
        plugins: { 
            legend: { display: false },
            datalabels: {
                color: '#000',
                font: { weight: 'bold' },
                anchor: 'end',
                align: 'right'
            },
            title: { 
                display: true,
                text: "Malicious Behavior by Username",
                font: { size: 18, weight: 'bold' } 
            }
        },
        onClick: (evt) => {
            const points = logUserChart.getElementsAtEventForMode(evt, 'nearest', { intersect: true }, true);
            if (points.length > 0) {
                const index = points[0].index;
                const label = logUserChart.data.labels[index];
                console.log("Clicked label:", label);
                highlightRowUser(label);
            }
        }
    },
    plugins: [ChartDataLabels]
});

const logTimeCtx = document.getElementById('timeHistogram').getContext('2d');
const logTimeChart = new Chart(logTimeCtx, {
    type: 'bar',
    data: {
        labels: [)" << timeLabels << R"(],
        datasets: [{
            label: 'Event Count',
            data: [)" << timeData << R"(],
            backgroundColor: 'rgba(255,99,132,0.6)'
        }]
    },
    options: {
        responsive: false,
        plugins: {
            legend: { display: false },
            datalabels: {
                color: '#000',
                font: { weight: 'bold' },
                anchor: 'end',
                align: 'top'
            },
            title: {
                display: true,
                text: "Malicious Behavior By Time",
                font: { size: 18, weight: 'bold' }
            }
        },
        onClick: (evt) => {
            const points = logTimeChart.getElementsAtEventForMode(evt, 'nearest', { intersect: true }, true);
            if (points.length > 0) {
                const index = points[0].index;
                const label = logTimeChart.data.labels[index];
                console.log("Clicked label:", label);
                highlightRowTime(label);
            }
        },
        scales: {
            x: {
                title: {
                    display: false,
                    text: 'Time Interval (3 hours)'
                }
            },
            y: {
                beginAtZero: true,
                title: {
                    display: false,
                    text: 'Number of Events'
                }
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