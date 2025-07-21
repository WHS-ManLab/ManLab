#include "FimLogToDB.h"
#include "DBManager.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <regex>

//각 항목 로그 파싱
bool FimLogToDB::parseLogLine(const std::string& line, ParsedLog& outLog)
{
    std::regex pattern(R"(\[(.*?)\]\s+\[file_logger\]\s+\[info\]\s+\[Event Type\] = ([^\[]+?)\s+\[Path\] =\s+(.+))");


    std::smatch match;

    if (std::regex_match(line, match, pattern)) 
    {
        outLog.timestamp = match[1].str();   // "2025-07-15 17:43:07.697"
        outLog.eventType = match[2].str();   // "ATTRIB CHANGE"
        outLog.path = match[3].str();        // "/home/b.txt"
        return true;
    } 
    else 
    {
        return false;
    }
}

void FimLogToDB::ParseAndStore(const std::string& logFilePath, const std::string& lastSavedTime) {
    std::ifstream logFile(logFilePath);
    if (!logFile.is_open()) {
        std::cerr << "[ERROR] 로그 파일 열기 실패: " << logFilePath << std::endl;
        return;
    }

    auto& storage = DBManager::GetInstance().GetRealTimeMonitorStorage();

    std::string line;
    std::string newestTimestamp = lastSavedTime; // 갱신된 최신 시간 기록용

    while (std::getline(logFile, line)) {
        ParsedLog parsed;
        if (parseLogLine(line, parsed)) {
            if (parsed.timestamp > lastSavedTime) { 
                RealtimeEventLog event;
                event.eventType = parsed.eventType;
                event.path = parsed.path;
                event.timestamp = parsed.timestamp;

                try {
                    storage.insert(event);
                    if (parsed.timestamp > newestTimestamp) {
                        newestTimestamp = parsed.timestamp;
                    }
                } catch (const std::exception& e) {
                    std::cerr << "[ERROR] DB 저장 실패: " << e.what() << std::endl;
                }
            }
        }
    }
}

//디비 갱신을 위한 가장 최근 저장되 타임스탬프
std::string FimLogToDB::GetLatestTimestamp() {
    auto& storage = DBManager::GetInstance().GetRealTimeMonitorStorage();
    
    // 전체 행 가져와서 시간순 정렬 후 가장 마지막 1개 선택
    auto rows = storage.select(&RealtimeEventLog::timestamp);

    if (!rows.empty()) {
        // 문자열 기준으로 가장 큰 timestamp가 최신일 것이라 가정
        return *std::max_element(rows.begin(), rows.end());
    } else {
        return "0000-00-00 00:00:00.000";
    }
}
