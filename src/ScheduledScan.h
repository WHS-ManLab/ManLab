#pragma once

#include <string>
#include <vector>
#include <chrono>

enum class eScheduleType {
    Monthly,
    Weekly,
    SpecificDate
};

struct ScanSchedule {
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

    std::chrono::system_clock::time_point GetNextTriggerTime();
    void WaitUntil(const std::chrono::system_clock::time_point& time);
    void RunScan();

private:
    std::vector<ScanSchedule> mSchedules;

    void loadSchedules();
    std::chrono::system_clock::time_point calculateNextTime(const ScanSchedule& schedule);
};
