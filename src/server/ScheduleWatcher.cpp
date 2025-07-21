#include "ScheduleWatcher.h"
#include "ScheduleParser.h"
#include "Paths.h"
#include <INIReader.h>
#include <chrono>
#include <thread>
#include <ctime>
#include <spdlog/spdlog.h>

using namespace manlab::utils;

void ScheduleWatcher::Init(std::atomic<bool>& shouldRun,
                           std::atomic<bool>& scanRequested,
                           std::atomic<bool>& reportRequested,
                           std::mutex& scanMutex,
                           std::condition_variable& scanCondVar,
                           std::mutex& reportMutex,
                           std::condition_variable& reportCondVar)
{
    mpShouldRun = &shouldRun;

    mpScanRequested = &scanRequested;
    mpScanMutex = &scanMutex;
    mpScanCondVar = &scanCondVar;

    mpReportRequested = &reportRequested;
    mpReportMutex = &reportMutex;
    mpReportCondVar = &reportCondVar;
}

void ScheduleWatcher::Run()
{
    spdlog::info("ScheduleWatcher::Run() 시작");

    while (*mpShouldRun)
    {
        std::time_t now = std::time(nullptr);
        std::tm localNow;
        localtime_r(&now, &localNow);
        bool Execute = false;


        // 검사 조건 체크
        {
            spdlog::debug("검사 스케줄 설정 파일 로딩 중: {}", PATH_SCHEDUL_CONFIG_INI);
            INIReader reader(PATH_SCHEDUL_CONFIG_INI);
            if (reader.ParseError() != 0)
            {
                spdlog::warn("스케줄 INI 파싱 실패");
            }
            else
            {
                std::string typeStr = reader.Get("ScanSchedul", "Type", "");
                std::string timeStr = reader.Get("ScanSchedul", "Time", "");

                GeneralSchedule schedule;
                bool parsed = ParseScheduleFromINI("ScanSchedul", typeStr, timeStr, schedule);
                if (parsed)
                {
                    spdlog::debug("스케줄 파싱 성공 (ScanSchedul): Type={}, Time={}", typeStr, timeStr);
                    if (IsTimeToTrigger(schedule, localNow))
                    {
                        {
                            std::lock_guard<std::mutex> lock(*mpScanMutex);
                            *mpScanRequested = true;
                        }
                        mpScanCondVar->notify_one();
                        Execute = true;
                        spdlog::info("예약 검사 조건 만족 → 검사 요청 전달됨");
                    }
                }
                else
                {
                    spdlog::warn("스케줄 파싱 실패 (ScanSchedul)");
                }
            }
        }

        // 리포트 조건 체크
        {
            spdlog::debug("리포트 설정 파일 로딩 중: {}", PATH_LOG_REPORT_INI);
            INIReader reader(PATH_LOG_REPORT_INI);
            if (reader.ParseError() != 0)
            {
                spdlog::warn("리포트 INI 파싱 실패");
            }
            else
            {
                std::string typeStr = reader.Get("Report", "Type", "");
                std::string timeStr = reader.Get("Report", "Time", "");

                GeneralSchedule schedule;
                bool parsed = ParseScheduleFromINI("Report", typeStr, timeStr, schedule);
                if (parsed)
                {
                    spdlog::debug("스케줄 파싱 성공 (Report): Type={}, Time={}", typeStr, timeStr);
                    if (IsTimeToTrigger(schedule, localNow))
                    {
                        {
                            std::lock_guard<std::mutex> lock(*mpReportMutex);
                            *mpReportRequested = true;
                        }
                        mpReportCondVar->notify_one();
                        Execute = true;
                        spdlog::info("예약 리포트 조건 만족 → 리포트 요청 전달됨");
                    }   
                }
                else
                {
                    spdlog::warn("스케줄 파싱 실패 (Report)");
                }
            }
        }

        // 종료 확인
        if (!*mpShouldRun)
        {
            spdlog::info("ScheduleWatcher::Run() 종료 요청 감지됨");
            break;
        }

        // 대기시간 결정
        if(Execute) 
        {
            spdlog::debug("60초 후 재확인 예정");
            std::this_thread::sleep_for(std::chrono::seconds(60));
        }
        else 
        {
            spdlog::debug("30초 후 재확인 예정");
            std::this_thread::sleep_for(std::chrono::seconds(30));
        }
    }
    spdlog::info("ScheduleWatcher::Run() 정상 종료됨");
}