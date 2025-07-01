#include "ScheduledScanDaemon.h"
#include "MalwareScan.h"
#include "INIReader.h"   
#include <thread>
#include <chrono>
#include <sstream>
#include <iostream>
#include <unordered_map>

using namespace std;

void ScheduledScanDaemon::run()
{
    daemonize();
    handleSignals();

    while (running) {
        auto now = chrono::system_clock::now();
        auto nextTrigger = getNextTriggerTime(now);

        if (nextTrigger > now) {
            this_thread::sleep_until(nextTrigger);
        }

        if (!running) break;  

        MalwareScan scanner;
        scanner.run(); 
        std::this_thread::sleep_for(std::chrono::minutes(1));
    }
}

static tm toTm(const chrono::system_clock::time_point& tp)
{
    time_t t = chrono::system_clock::to_time_t(tp);
    tm result;
    localtime_r(&t, &result);  // thread-safe
    return result;
}

static chrono::minutes getTimeOfDay(const chrono::system_clock::time_point& tp)
{
    tm t = toTm(tp);
    return chrono::minutes(t.tm_hour * 60 + t.tm_min);
}

static chrono::system_clock::time_point computeNextMonthly(const ScanSchedule& s, const chrono::system_clock::time_point& now)
{
    tm nowTm = toTm(now);
    tm targetTm = nowTm;

    targetTm.tm_mday = s.nDayOfMonth;
    targetTm.tm_hour = s.tTimeOfDay.count() / 60;
    targetTm.tm_min  = s.tTimeOfDay.count() % 60;
    targetTm.tm_sec  = 0;

    time_t t = mktime(&targetTm);
    if (t == -1) return now + chrono::hours(24 * 365);

    auto result = chrono::system_clock::from_time_t(t);

    if (result <= now) {
        targetTm.tm_mon += 1;
        t = mktime(&targetTm);
        result = chrono::system_clock::from_time_t(t);
    }

    return result;
}

static chrono::system_clock::time_point computeNextWeekly(const ScanSchedule& s, const chrono::system_clock::time_point& now)
{
    tm nowTm = toTm(now);
    int currentWeekday = nowTm.tm_wday; // Sunday = 0
    int targetWeekday = s.nDayOfWeek;

    int daysToAdd = (targetWeekday - currentWeekday + 7) % 7;
    if (daysToAdd == 0 && s.tTimeOfDay <= getTimeOfDay(now)) {
        daysToAdd = 7;
    }

    auto targetDay = now + chrono::hours(24 * daysToAdd);
    tm targetTm = toTm(targetDay);

    targetTm.tm_hour = s.tTimeOfDay.count() / 60;
    targetTm.tm_min  = s.tTimeOfDay.count() % 60;
    targetTm.tm_sec  = 0;

    time_t t = mktime(&targetTm);
    return chrono::system_clock::from_time_t(t);
}

static chrono::system_clock::time_point computeNextSpecific(const ScanSchedule& s, const chrono::system_clock::time_point& now)
{
    return (s.tDateTime > now) ? s.tDateTime : now + chrono::hours(24 * 365);
}

chrono::system_clock::time_point ScheduledScanDaemon::getNextTriggerTime(const chrono::system_clock::time_point& now)
{
    chrono::system_clock::time_point nearest = now + chrono::hours(24 * 365);

    for (const auto& schedule : mScanSchedules) {
        chrono::system_clock::time_point next;

        switch (schedule.eType) {
        case eScheduleType::Monthly:
            next = computeNextMonthly(schedule, now);
            break;
        case eScheduleType::Weekly:
            next = computeNextWeekly(schedule, now);
            break;
        case eScheduleType::SpecificDate:
            next = computeNextSpecific(schedule, now);
            break;
        }

        if (next < nearest)
            nearest = next;
    }

    return nearest;
}

void ScheduledScanDaemon::loadScheduleFromIni()
{
    const string iniPath = "/ManLab/conf/ScanSchedul.ini";
    INIReader reader(iniPath);

    if (reader.ParseError() != 0) {
        cerr << "Failed to parse schedule ini: using no schedule" << endl;
        return;
    }

    // [Monthly]
    string mDayStr = reader.Get("Monthly", "Day", "");
    string mTimeStr = reader.Get("Monthly", "Time", "");
    if (!mDayStr.empty() && !mTimeStr.empty()) {
        ScanSchedule s;
        s.eType = eScheduleType::Monthly;
        s.nDayOfMonth = stoi(mDayStr);

        int hour = 0, minute = 0;
        sscanf(mTimeStr.c_str(), "%d:%d", &hour, &minute);
        s.tTimeOfDay = chrono::minutes(hour * 60 + minute);

        mScanSchedules.push_back(s);
    }

    // [Weekly]
    string wDayStr = reader.Get("Weekly", "Day", "");
    string wTimeStr = reader.Get("Weekly", "Time", "");
    if (!wDayStr.empty() && !wTimeStr.empty()) {
        unordered_map<string, int> weekdays = {
            {"Sunday", 0}, {"Monday", 1}, {"Tuesday", 2}, {"Wednesday", 3},
            {"Thursday", 4}, {"Friday", 5}, {"Saturday", 6}
        };

        auto it = weekdays.find(wDayStr);
        if (it != weekdays.end()) {
            ScanSchedule s;
            s.eType = eScheduleType::Weekly;
            s.nDayOfWeek = it->second;

            int hour = 0, minute = 0;
            sscanf(wTimeStr.c_str(), "%d:%d", &hour, &minute);
            s.tTimeOfDay = chrono::minutes(hour * 60 + minute);

            mScanSchedules.push_back(s);
        }
    }

    // [SpecificDate]
    string dDateStr = reader.Get("SpecificDate", "Date", "");
    string dTimeStr = reader.Get("SpecificDate", "Time", "");
    if (!dDateStr.empty() && !dTimeStr.empty()) {
        int y, m, d, hour, min;
        if (sscanf(dDateStr.c_str(), "%d-%d-%d", &y, &m, &d) == 3 &&
            sscanf(dTimeStr.c_str(), "%d:%d", &hour, &min) == 2) {

            tm t = {};
            t.tm_year = y - 1900;
            t.tm_mon = m - 1;
            t.tm_mday = d;
            t.tm_hour = hour;
            t.tm_min = min;
            t.tm_sec = 0;

            time_t time = mktime(&t);
            if (time != -1) {
                ScanSchedule s;
                s.eType = eScheduleType::SpecificDate;
                s.tDateTime = chrono::system_clock::from_time_t(time);
                mScanSchedules.push_back(s);
            }
        }
    }
}
