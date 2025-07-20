#include "ScheduleParser.h"
#include "StringUtils.h"
#include "INIReader.h"

#include <sstream>
#include <iomanip>
#include <map>
#include <ctime>  // std::tm

namespace manlab::utils {

bool IsTimeToTrigger(const GeneralSchedule& schedule, const std::tm& now)
{
    switch (schedule.type)
    {
    case ScheduleType::Daily:
        return (now.tm_hour == schedule.hour && now.tm_min == schedule.minute);

    case ScheduleType::Weekly:
        return (now.tm_wday == schedule.dayOfWeek &&
                now.tm_hour == schedule.hour &&
                now.tm_min == schedule.minute);

    case ScheduleType::Monthly:
        return (now.tm_mday == schedule.dayOfMonth &&
                now.tm_hour == schedule.hour &&
                now.tm_min == schedule.minute);

    case ScheduleType::SpecificDate:
    {
        std::tm scheduledTm{};
        if (ParseDateString(schedule.dateString, scheduledTm)) 
        {
            return (now.tm_year == scheduledTm.tm_year &&
                    now.tm_mon == scheduledTm.tm_mon &&
                    now.tm_mday == scheduledTm.tm_mday &&
                    now.tm_hour == schedule.hour &&
                    now.tm_min == schedule.minute);
        }
        return false;
    }

    default:
        return false;
    }
}

// "HH:MM" → hour/minute 분해
bool ParseHourMinute(const std::string& timeStr, int& hour, int& minute)
{
    std::stringstream ss(timeStr);
    std::string hh, mm;
    if (std::getline(ss, hh, ':') && std::getline(ss, mm))
    {
        try 
        {
            hour = std::stoi(hh);
            minute = std::stoi(mm);
            return true;
        } catch (...) {}
    }
    return false;
}

// "YYYY-MM-DD" → std::tm 구조체 파싱
bool ParseDateString(const std::string& dateStr, std::tm& outTm)
{
    std::istringstream ss(dateStr);
    ss >> std::get_time(&outTm, "%Y-%m-%d");
    return !ss.fail();
}

// INI 파일의 Type/Time 키 → GeneralSchedule 구조체 파싱
bool ParseScheduleFromINI(const std::string& sectionName,
                          const std::string& typeStr,
                          const std::string& timeStr,
                          GeneralSchedule& out)
{
    static const std::map<std::string, int> kDayMap = 
    {
        {"Sunday", 0}, {"Monday", 1}, {"Tuesday", 2}, {"Wednesday", 3},
        {"Thursday", 4}, {"Friday", 5}, {"Saturday", 6}
    };

    out = GeneralSchedule{};

    std::string trimmedType = trim(typeStr);
    std::string trimmedTime = trim(timeStr);
    if (!ParseHourMinute(trimmedTime, out.hour, out.minute))
    {
        return false;
    }

    // ':' 기준으로 쪼개기
    std::string typePart, argPart;
    auto pos = trimmedType.find(':');
    if (pos != std::string::npos) 
    {
        typePart = trimmedType.substr(0, pos);
        argPart  = trimmedType.substr(pos + 1);
    } 
    else // daliy
    {
        typePart = trimmedType;
        argPart  = "";
    }

    if (typePart == "daily") 
    {
        out.type = ScheduleType::Daily;
        return true;
    }

    if (typePart == "weekly") 
    {
        auto it = kDayMap.find(argPart);
        if (it != kDayMap.end()) 
        {
            out.type = ScheduleType::Weekly;
            out.dayOfWeek = it->second;
            return true;
        }
    }

    if (typePart == "monthly") {
        try 
        {
            out.type = ScheduleType::Monthly;
            out.dayOfMonth = std::stoi(argPart);
            return true;
        } catch (...) 
        {
            return false;
        }
    }

    if (typePart == "date") 
    {
        std::tm tmp{};
        if (ParseDateString(argPart, tmp)) 
        {
            out.type = ScheduleType::SpecificDate;
            out.dateString = argPart;
            return true;
        }
    }

    return false;  // Unknown or invalid
}

}  // namespace manlab::utils