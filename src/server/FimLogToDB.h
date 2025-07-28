#pragma once
#include <string>

class FimLogToDB {
public:
    // 로그 파일 경로를 받아 파싱 후 DB에 저장
     void ParseAndStore(const std::string& logFilePath, const std::string& lastSavedTime);
     std::string GetLatestTimestamp();

private:
    struct ParsedLog {
    std::string timestamp;
    std::string eventType;
    std::string path;     
    std::string newName;  
    std::string md5;    
};

    // 로그 한 줄을 파싱해서 ParsedLog 구조체에 저장하는 함수
    bool parseLogLine(const std::string& line, ParsedLog& outLog);
};