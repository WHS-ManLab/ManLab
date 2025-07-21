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

using namespace std::chrono;
using manlab::utils::stripComment;
using manlab::utils::trim;

bool ReportService::loadEmailSettings()
{
    INIReader reader(PATH_LOG_REPORT_INI);
    if (reader.ParseError() < 0)
    {
        return false;
    }

    std::string strEnabled = trim(stripComment(reader.Get("Email", "Enabled", "false")));
    mbIsEnabled = (strEnabled == "true");

    mRecipient = trim(stripComment(reader.Get("Email", "Recipient", "")));

    return true;
}

bool ReportService::Run()
{
    loadEmailSettings();

    mCurrentTime = getCurrentTimeString();
    std::string htmlFile = std::string(PATH_LOG_REPORT) + "/Report_" + mCurrentTime.substr(0, 10) + ".html";

    auto events = fetchData(mLastReportTime, mCurrentTime);
    if (generateHTML(htmlFile, events))
    {
        mLastReportTime = mCurrentTime;

        if (mbIsEnabled)
        {
            GmailClient client(mRecipient);
            if(client.Run(htmlFile))
            {
                return true;
            }
        }
    }
    return false;
    // TODO : HTML 생성 실패 / 루프 종료 로직
}

std::vector<LogAnalysisResult> ReportService::fetchData(const std::string &fromTime, const std::string &toTime)
{
    auto &storage = DBManager::GetInstance().GetLogAnalysisResultStorage();
    return storage.get_all<LogAnalysisResult>(
        sqlite_orm::where(sqlite_orm::between(&LogAnalysisResult::timestamp, fromTime, toTime)));
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
<p>Report generated: )";

    html << mCurrentTime << "</p>\n";

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
            <th>User ID</th>
            <th>Timestamp</th>
            <th>Success</th>
            <th>Original Log Path</th>
            <th>Description</th>
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

        for (const auto &e : events)
        {
            typeCounts[e.type]++;

            html << "<tr>";
            html << "<td>" << e.id << "</td>";
            html << "<td>" << e.type << "</td>";
            html << "<td>" << e.uid << "</td>";
            html << "<td>" << e.timestamp << "</td>";
            html << "<td>" << (e.bIsSuccess ? "Yes" : "No") << "</td>";
            html << "<td>" << e.originalLogPath << "</td>";
            html << "<td>" << e.description << "</td>";
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

        for (const auto &e : events)
        {
            html << "<tr>";
            html << "<td>" << e.id << "</td>";
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

    html << R"(
<h1>Malware Scan Report</h1>
<p>Scan records from )" << mLastReportTime << " to " << mCurrentTime << R"(</p>
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
        sqlite_orm::where(sqlite_orm::between(&ScanReport::date, mLastReportTime, mCurrentTime)));

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