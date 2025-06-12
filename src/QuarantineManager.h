#pragma once

#include <filesystem>
#include <sqlite3.h>    // SQLite 연결을 위해 사용
#include <string>
#include <vector>

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
    bool IsSuccess;              // 격리 성공 여부(True / false)
};

class QuarantineManager
{
private:
    // 멤버 변수 선언
    // 스캔 시, 악성코드 파일이 여러개가 탐지되는 것을 가정하여, 데이터 형을 vector로 했습니다.
    std::vector<std::string> mFilesToQuarantine; // 원래 파일 경로
    std::vector<std::string> mQuarantineReasons; // 격리된 원인("hash" 또는 "YARA")
    std::vector<std::string> mMalwareNamesOrRules;   // 악성코드명 또는 탐지 룰 이름
    std::vector<long long> mOriginalFileSizes;   // 원래 파일 크기

    std::vector<bool> mbIsQuarantineSuccess;  // 격리 성공 여부

    std::string mQuarantineDir;  // 악성코드 파일을 격리할 디렉토리 경로
    std::string mMetadataDbPath; // 격리 메타데이터 DB 파일 경로
    sqlite3* mDb; // SQLite DB 연결 상태를 저장하는 포인터

    // 악성코드 파일 격리 시, 암호화(XOR) 키
    static constexpr unsigned char ENCRYPTION_KEY = 0xA5;  // 키 값은 임의로 작성하였습니다.

    // 멤버 함수 선언
    bool openDatabase();    // DB 연결
    void closeDatabase();   // DB 연결 종료
    bool logQuarantineMetadata(const QuarantineMetadata& metadata); // DB에 격리 정보 기록(리포트, 로그를 위해서) 함수
    bool applySimpleXOREncryption(const fs::path& filePath); // 악성코드 파일 암호화 함수
    // 악성코드 파일 격리 과정에 대한 함수(격리 디렉토리 생성 확인, 파일 이동, 암호화 등)
    bool processQuarantine(const std::string& originalPath, const std::string& reason, const std::string& nameOrRule, long long originalSize);
    std::string getCurrentDateTime() const; // 현재 시간 문자열로 가져오는 함수

public:
    // 생성자(QuarantineManager 객체를 만들 때 호출됨)
    QuarantineManager(const std::vector<std::string>& filesToQuarantine,
                      const std::vector<std::string>& reasons,
                      const std::vector<std::string>& namesOrRules,
                      const std::vector<long long>& sizes,
                      const std::string& quarantineDirectory = "/quarantine_zone", // 격리 디렉토리 경로(임의 작성)
                      const std::string& metadataDatabasePath = "/db/quarantine_metadata.db"); // DB 파일 경로 (임의 작성)

    // 소멸자(QuarantineManager 객체가 사라질 때 호출됨)
    ~QuarantineManager();

    // 격리 작업을 시작하는 함수
    void Run();

    // 악성코드 파일의 격리 성공 여부 목록을 반환하는 함수
    const std::vector<bool>& GetIsQuarantineSuccess() const;
};
