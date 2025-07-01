#pragma once

#include "DaemonBase.h"
#include <vector>
#include <chrono>
#include <string>

enum class eScheduleType
{
    Monthly,
    Weekly,
    SpecificDate
};

struct ScanSchedule
{
    eScheduleType eType;
    int nDayOfMonth = 0;   // 월간: 1~31
    int nDayOfWeek = 0;    // 주간: 0 = 일요일 ~ 6 = 토요일
    std::chrono::minutes tTimeOfDay{};  // 공통 시간
    std::chrono::system_clock::time_point tDateTime{}; // 특정일
};

class ScheduledScanDaemon : public DaemonBase {
public:
    void run() override;

private:
    std::vector<ScanSchedule> mScanSchedules;

    void loadScheduleFromIni();
    std::chrono::system_clock::time_point getNextTriggerTime(const std::chrono::system_clock::time_point& now);
};
