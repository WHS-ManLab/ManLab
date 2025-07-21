// ScheduleParser.h
#pragma once

#include <string>
#include <vector>
#include <ctime>

namespace manlab::utils {

enum class ScheduleType 
{
    Daily,
    Weekly,
    Monthly,
    SpecificDate,
    Invalid
};

struct GeneralSchedule 
{
    ScheduleType type;
    int dayOfWeek = -1;
    int dayOfMonth = -1;
    std::string dateString;
    int hour = -1;
    int minute = -1;
};

bool ParseHourMinute(const std::string& timeStr, int& hour, int& minute);
bool ParseDateString(const std::string& dateStr, std::tm& outTm);
bool IsTimeToTrigger(const GeneralSchedule& schedule, const std::tm& now);
bool ParseScheduleFromINI(const std::string& sectionName,
                          const std::string& typeStr,
                          const std::string& timeStr,
                          GeneralSchedule& out);

} // namespace manlab::utils