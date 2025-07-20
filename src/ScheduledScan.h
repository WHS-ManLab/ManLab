#pragma once

#include <string>
#include <vector>
#include <chrono>

enum class eScheduleType 
{
    Monthly,
    Weekly,
    SpecificDate
};

struct ScanSchedule 
{
    eScheduleType type;

    // Monthly
    int dayOfMonth = -1;

    // Weekly
    int dayOfWeek = -1; // 0 = Sunday

    // SpecificDate
    std::string dateString;

    // 공통
    int hour = 0;
    int minute = 0;
};

class ScheduledScan {
public:
    ScheduledScan();
    bool ShouldTriggerNow();
    void RunScan();

private:
    std::vector<ScanSchedule> mMonthlySchedules;
    std::vector<ScanSchedule> mWeeklySchedules;
    std::vector<ScanSchedule> mSpecificDateSchedules;

    void loadSchedules();
    void parseTime(const std::string& timeStr, int& hour, int& minute);
};