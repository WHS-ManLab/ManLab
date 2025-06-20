#pragma once

#include <filesystem>
#include <sqlite3.h>    // SQLite 연결을 위해 사용
#include <string>
#include <vector>

#include "ScanMalware.h" // DetectionResultRecord 구조체 선언 포함

namespace fs = std::filesystem;

// 격리된 파일의 메타데이터 구조체 선언
struct QuarantineMetadata
{
    std::string OriginalPath;    // 원래 파일 경로
    std::string QuarantinedPath; // 격리된 파일의 경로
    long long OriginalSize;      // 원래 파일 크기
    std::string QuarantineDate;  // 격리된 날짜와 시간
    std::string QuarantineReason; // 격리된 원인("hash" 또는 "YARA")
    std::string MalwareNameOrRule; // 악성코드명 또는 탐지 룰 이름
};

class QuarantineManager
{
private:
    std::vector<bool> mbIsQuarantineSuccess;  // 격리 성공 여부 (Run 메서드에서 채워짐)

    sqlite3* mDb; // SQLite DB 연결 상태를 저장하는 포인터

    // 악성코드 파일 격리 시, 암호화(XOR) 키
    static constexpr unsigned char ENCRYPTION_KEY = 0xA5;  // 키 값은 임의로 작성하였습니다.

    // 멤버 함수 선언
    bool openDatabase(const std::string& dbPath);    // DB 연결
    void closeDatabase();   // DB 연결 종료
    bool logQuarantineMetadata(const QuarantineMetadata& metadata); // DB에 격리 정보 기록
    bool applySimpleXOREncryption(const fs::path& filePath); // 악성코드 파일 암호화 함수
    // 악성코드 파일 격리 과정에 대한 함수
    bool processQuarantine(const DetectionResultRecord& item, const std::string& quarantineDirectory);
    std::string getCurrentDateTime() const; // 현재 시간 문자열로 가져오는 함수

public:
    QuarantineManager();

    ~QuarantineManager();

    // 격리 작업을 시작하는 함수
    void Run(const std::vector<DetectionResultRecord>& itemsToQuarantine,
             const std::string& quarantineDirectory,
             const std::string& metadataDatabasePath);

    // 악성코드 파일의 격리 성공 여부 목록을 반환하는 함수
    const std::vector<bool>& GetIsQuarantineSuccess() const;
};
