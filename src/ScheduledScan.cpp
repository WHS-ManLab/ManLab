#include "ScheduledScan.h"
#include "MalwareScan.h"
#include "StringUtils.h"
#include "INIReader.h"
#include "Paths.h"

#include <sstream>
#include <iomanip>
#include <ctime>
#include <thread>
#include <chrono>
#include <map>
#include <iostream>

using manlab::utils::trim;

// 객체 생성
ScheduledScan::ScheduledScan() {}

// INI파일 스캔 정보 
// [Monthly], [Weekly], [SpecificDate]를 읽어 구조체로 저장
bool ScheduledScan::ShouldTriggerNow()
{
    loadSchedules();

    std::time_t now_c = std::time(nullptr);
    std::tm now_tm = *std::localtime(&now_c);

    // Monthly
    for (const auto& s : mMonthlySchedules)
        if (now_tm.tm_mday == s.dayOfMonth &&
            now_tm.tm_hour == s.hour &&
            now_tm.tm_min  == s.minute)
            return true;

    // Weekly
    for (const auto& s : mWeeklySchedules)
        if (now_tm.tm_wday == s.dayOfWeek &&
            now_tm.tm_hour == s.hour &&
            now_tm.tm_min  == s.minute)
            return true;

    // SpecificDate
    for (const auto& s : mSpecificDateSchedules)
    {
        std::tm target_tm = {};
        std::istringstream ss(s.dateString);
        ss >> std::get_time(&target_tm, "%Y-%m-%d");
        if (now_tm.tm_year == target_tm.tm_year &&
            now_tm.tm_mon  == target_tm.tm_mon &&
            now_tm.tm_mday == target_tm.tm_mday &&
            now_tm.tm_hour == s.hour &&
            now_tm.tm_min  == s.minute)
            return true;
    }

    return false;
}


void ScheduledScan::RunScan()
{
    MalwareScan scan;
    scan.Init();
    scan.SetMode(MalwareScan::Mode::Scheduled); 
    scan.Run(nullptr);
    scan.SendNotification();
    scan.SaveReportToDB();
}

// (private) INI 파싱 → mSchedules 갱신
void ScheduledScan::loadSchedules()
{
    mMonthlySchedules.clear();
    mWeeklySchedules.clear();
    mSpecificDateSchedules.clear();

    INIReader reader(PATH_SCHEDUL_CONFIG_INI);
    if (reader.ParseError() != 0)
        return;

    // Monthly
    {
        std::string dayStr  = trim(reader.Get("Monthly", "Day", ""));
        std::string timeStr = trim(reader.Get("Monthly", "Time", ""));
        if (!dayStr.empty() && !timeStr.empty())
        {
            ScanSchedule s{};
            s.type = eScheduleType::Monthly;
            s.dayOfMonth = std::stoi(dayStr);
            parseTime(timeStr, s.hour, s.minute);
            mMonthlySchedules.push_back(s);
        }
    }

    // Weekly
    {
        static const std::map<std::string, int> kDayMap = {
            {"Sunday",0},{"Monday",1},{"Tuesday",2},{"Wednesday",3},
            {"Thursday",4},{"Friday",5},{"Saturday",6}
        };

        std::string dayStr = trim(reader.Get("Weekly", "Day", ""));
        std::string timeStr = trim(reader.Get("Weekly", "Time", ""));
        if (!dayStr.empty() && !timeStr.empty())
        {
            auto it = kDayMap.find(dayStr);
            if (it != kDayMap.end())
            {
                ScanSchedule s{};
                s.type = eScheduleType::Weekly;
                s.dayOfWeek = it->second;
                parseTime(timeStr, s.hour, s.minute);
                mWeeklySchedules.push_back(s);
            }
        }
    }

    // SpecificDate
    {
        std::string dateStr = trim(reader.Get("SpecificDate", "Date", ""));
        std::string timeStr = trim(reader.Get("SpecificDate", "Time", ""));
        if (!dateStr.empty() && !timeStr.empty())
        {
            ScanSchedule s{};
            s.type = eScheduleType::SpecificDate;
            s.dateString = dateStr;
            parseTime(timeStr, s.hour, s.minute);
            mSpecificDateSchedules.push_back(s);
        }
    }
}


// (private helper) "HH:MM" → hour/minute 분해
void ScheduledScan::parseTime(const std::string& timeStr, int& hour, int& minute)
{
    std::stringstream ss(timeStr);
    std::string hh, mm;
    if (std::getline(ss, hh, ':') && std::getline(ss, mm))
    {
        hour   = std::stoi(hh);
        minute = std::stoi(mm);
    }
}
