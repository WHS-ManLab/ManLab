#include "FimLogToDB.h"
#include "DBManager.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <regex>
#include <optional>

//각 항목 로그 파싱
bool FimLogToDB::parseLogLine(const std::string& line, ParsedLog& outLog)
{
    std::smatch match;

    // RENAME 로그
    std::regex renamePattern(R"(\[(.*?)\]\s+\[RealTime_logger\]\s+\[info\]\s+\[Event Type\] =\s*RENAME\s+\[From\] =\s*(.*?)\s*->\s*\[To\] =\s*(.+))");

    std::regex generalPattern(R"(\[(.*?)\]\s+\[RealTime_logger\]\s+\[info\]\s+\[Event Type\] =\s*(\S+)\s+\[Path\] =\s+(.+))");

    if (std::regex_match(line, match, renamePattern)) 
    {
        outLog.timestamp = match[1].str();
        outLog.eventType = "RENAME";
        outLog.path = match[2].str();       // From 경로
        outLog.newName = match[3].str();    // To 경로
    return true;
    } 
    else if (std::regex_match(line, match, generalPattern)) 
    {
    outLog.timestamp = match[1].str();
    outLog.eventType = match[2].str();
    outLog.path = match[3].str();
    outLog.newName = "-";                
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
    std::string newestTimestamp = lastSavedTime;

    while (std::getline(logFile, line)) {
        ParsedLog parsed;
        if (parseLogLine(line, parsed)) {
            if (parsed.timestamp > lastSavedTime) {
                RealtimeEventLog event;
                event.eventType = parsed.eventType;
                event.timestamp = parsed.timestamp;
                event.path = parsed.path;
                event.newName = parsed.newName;

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


std::string FimLogToDB::GetLatestTimestamp() {
    auto& storage = DBManager::GetInstance().GetRealTimeMonitorStorage();
    auto rows = storage.select(&RealtimeEventLog::timestamp);

    if (!rows.empty()) {
        return *std::max_element(rows.begin(), rows.end());
    } else {
        return "0000-00-00 00:00:00.000";
    }
}
