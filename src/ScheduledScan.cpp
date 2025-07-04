#include "ScheduledScan.h"
#include "MalwareScan.h"
#include "StringUtils.h"
#include "INIReader.h"

#include <sstream>
#include <iomanip>
#include <ctime>
#include <thread>
#include <chrono>
#include <map>

#include <iostream>

using manlab::utils::trim;

//객체 생성 시 INI파일 스캔 정보 읽기
ScheduledScan::ScheduledScan()
{
    loadSchedules();
}

// INI파일 스캔 정보 
// [Monthly], [Weekly], [SpecificDate]를 읽어 구조체로 저장
void ScheduledScan::loadSchedules()
{
    const std::string iniPath = "/ManLab/conf/ScanSchedul.ini";
    INIReader reader(iniPath);

    if (reader.ParseError() != 0)
    {
        // TODO: 설정 파일 로딩 실패 시 로깅
        return;
    }

    // [Monthly] 파싱
    {
        // 1. 매달 며칠에 실행할 것인지
        // 2. 어떤 시간에 실행할 것인지
        std::string dayStr = trim(reader.Get("Monthly", "Day", ""));
        std::string timeStr = trim(reader.Get("Monthly", "Time", ""));

        // day, h, m 정수로 변환하여 저장
        if (!dayStr.empty() && !timeStr.empty())
        {
            ScanSchedule sched;
            sched.type = eScheduleType::Monthly;
            sched.dayOfMonth = std::stoi(dayStr);

            std::stringstream ss(timeStr);
            std::string hh;
            std::string mm;
            if (std::getline(ss, hh, ':') && std::getline(ss, mm))
            {
                sched.hour = std::stoi(hh);
                sched.minute = std::stoi(mm);
                mSchedules.push_back(sched);
            }
        }
    }

    // [Weekly] 파싱
    {
        // 요일 <-> 숫자 mapping table
        static const std::map<std::string, int> kDayMap = {
            {"Sunday", 0}, {"Monday", 1}, {"Tuesday", 2},
            {"Wednesday", 3}, {"Thursday", 4}, {"Friday", 5}, {"Saturday", 6}
        };

        // 1. 무슨 요일에 실행할 것인지
        // 2. 어떤 시간에 실행할 것인지
        std::string dayStr = trim(reader.Get("Weekly", "Day", ""));
        std::string timeStr = trim(reader.Get("Weekly", "Time", ""));

        // 요일, h, m을 정수로 변환하여 저장
        if (!dayStr.empty() && !timeStr.empty())
        {
            std::map<std::string, int>::const_iterator it = kDayMap.find(dayStr);
            if (it != kDayMap.end())
            {
                ScanSchedule sched;
                sched.type = eScheduleType::Weekly;
                sched.dayOfWeek = it->second;

                std::stringstream ss(timeStr);
                std::string hh;
                std::string mm;
                if (std::getline(ss, hh, ':') && std::getline(ss, mm))
                {
                    sched.hour = std::stoi(hh);
                    sched.minute = std::stoi(mm);
                    mSchedules.push_back(sched);
                }
            }
        }
    }

    // [SpecificDate] 파싱
    {
        // 1. 어떤 날짜에 실행할 것인지
        // 2. 어떤 시간에 실행할 것인지
        std::string dateStr = trim(reader.Get("SpecificDate", "Date", ""));
        std::string timeStr = trim(reader.Get("SpecificDate", "Time", ""));

        // 날짜는 문자열 형태로 저장, h, m은 숫자로 변환하여 저장
        if (!dateStr.empty() && !timeStr.empty())
        {
            ScanSchedule sched;
            sched.type = eScheduleType::SpecificDate;
            sched.dateString = dateStr;

            std::stringstream ss(timeStr);
            std::string hh;
            std::string mm;
            if (std::getline(ss, hh, ':') && std::getline(ss, mm))
            {
                sched.hour = std::stoi(hh);
                sched.minute = std::stoi(mm);
                mSchedules.push_back(sched);
            }
        }
    }
}

// INI파일에 존재하는 시간 중 현재 시각과 가장 가까운 예약 시간 계산
std::chrono::system_clock::time_point ScheduledScan::GetNextTriggerTime()
{
    // 현재 시간 변수, 가장 가까운 예약 시간 변수
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    std::chrono::system_clock::time_point nearest = std::chrono::system_clock::time_point::max();

    for (size_t i = 0; i < mSchedules.size(); ++i)
    {
        // C++ 시스템 시간으로 변환
        std::chrono::system_clock::time_point candidate = calculateNextTime(mSchedules[i]);

        // 가장 가까운 시간 찾기
        if (candidate > now && candidate < nearest)
        {
            nearest = candidate;
        }
    }

    return nearest;
}

// nearest time을 받아 해당 시간만큼 sleep
void ScheduledScan::WaitUntil(const std::chrono::system_clock::time_point& time)
{
    
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    if (time > now)
    {
        std::chrono::duration<double> duration = time - now;
        std::this_thread::sleep_for(duration);
    }
}

void ScheduledScan::RunScan()
{
    MalwareScan scan;
    scan.Run();
    // TODO: 데이터베이스에 결과 기록
}

// 다음 실행 시각을 std::chrono::system_clock::time_point로 계산해 반환
std::chrono::system_clock::time_point ScheduledScan::calculateNextTime(const ScanSchedule& schedule)
{
    using namespace std::chrono;

    std::time_t now_c = system_clock::to_time_t(system_clock::now());
    std::tm now_tm = *std::localtime(&now_c);
    std::tm target_tm = now_tm;

    target_tm.tm_sec = 0;
    target_tm.tm_min = schedule.minute;
    target_tm.tm_hour = schedule.hour;

    switch (schedule.type)
    {
        case eScheduleType::Monthly:
            target_tm.tm_mday = schedule.dayOfMonth;
            if (now_tm.tm_mday > schedule.dayOfMonth ||
                (now_tm.tm_mday == schedule.dayOfMonth &&
                 (now_tm.tm_hour > schedule.hour ||
                  (now_tm.tm_hour == schedule.hour && now_tm.tm_min >= schedule.minute))))
            {
                target_tm.tm_mon += 1;
            }
            break;

        case eScheduleType::Weekly:
        {
            int today = now_tm.tm_wday;
            int delta = (schedule.dayOfWeek - today + 7) % 7;
            if (delta == 0 &&
                (now_tm.tm_hour > schedule.hour ||
                 (now_tm.tm_hour == schedule.hour && now_tm.tm_min >= schedule.minute)))
            {
                delta = 7;
            }
            target_tm.tm_mday += delta;
            break;
        }

        case eScheduleType::SpecificDate:
        {
            std::istringstream ss(schedule.dateString);
            ss >> std::get_time(&target_tm, "%Y-%m-%d");
            target_tm.tm_hour = schedule.hour;
            target_tm.tm_min = schedule.minute;
            target_tm.tm_sec = 0;

            std::time_t target_time = std::mktime(&target_tm);
            std::time_t now_time = std::mktime(&now_tm);
            if (target_time <= now_time)
            {
                return system_clock::time_point::max();
            }
            break;
        }

        default:
            return system_clock::time_point::max();
    }

    std::time_t result_time = std::mktime(&target_tm);
    if (result_time == -1)
    {
        // TODO: 시간 계산 실패 로깅
        return system_clock::time_point::max();
    }

    return system_clock::from_time_t(result_time);
}
