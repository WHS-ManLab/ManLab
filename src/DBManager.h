#pragma once
#include <string>
#include <sqlite_orm.h>

/*
============================================
자신의 팀 전용 테이블과 DB를 다음 순서로 정의하세요.

순서:
1. 팀에서 사용할 데이터 구조 정의 (DBManager.h)
2. 각각의 DB 파일에 대한 storage 타입 정의(DBManager.h)
3. GETTER와 Storage 타입 변수 정의(DBManager.h)
4. 생성자 초기화 리스트에 make_storage() 추가 (DBManager.cpp)
5. storage.sync_schema()호출 추가 (DBManager.cpp)
6. Makefile에 db 추가
============================================
*/

// 악성코드 해시 DB 테이블 구조
struct MalwareHashDB
{
    std::string Hash;
    std::string Algorithm;
    std::string MalwareName;
    std::string SecurityVendors;
    std::string HashLicense;
};

// 격리 메타데이터 DB 테이블 구조
struct QuarantineMetadata
{
    std::string OriginalPath;
    std::string QuarantinedFileName;
    long long OriginalSize;
    std::string QuarantineDate;
    std::string QuarantineReason;
    std::string MalwareNameOrRule;
};

// 로그 분석 결과 DB 테이블 구조
struct LogAnalysisResult
{
    int id;
    std::string type;
    std::string description;
    std::string timestamp;
    std::string uid;
    bool bIsSuccess;
    std::string originalLogPath;
    std::string rawLine;
};

// FIM Baseline 테이블 구조
struct BaselineEntry
{
    std::string path;
    std::string md5;
};

// 악성코드 해시에 대한 storage 타입 정의
using StorageHash = decltype(sqlite_orm::make_storage("",
    sqlite_orm::make_table("MalwareHashDB",
        sqlite_orm::make_column("Hash", &MalwareHashDB::Hash),
        sqlite_orm::make_column("Algorithm", &MalwareHashDB::Algorithm),
        sqlite_orm::make_column("MalwareName", &MalwareHashDB::MalwareName),
        sqlite_orm::make_column("SecurityVendors", &MalwareHashDB::SecurityVendors),
        sqlite_orm::make_column("HashLicense", &MalwareHashDB::HashLicense)
    )
));

// 악성코드 격리 메타데이터에 대한 storage 타입 정의
using StorageQuarantine = decltype(sqlite_orm::make_storage("",
    sqlite_orm::make_table("QuarantineMetadata",
        sqlite_orm::make_column("OriginalPath", &QuarantineMetadata::OriginalPath),
        sqlite_orm::make_column("QuarantinedFileName", &QuarantineMetadata::QuarantinedFileName),
        sqlite_orm::make_column("OriginalSize", &QuarantineMetadata::OriginalSize),
        sqlite_orm::make_column("QuarantineDate", &QuarantineMetadata::QuarantineDate),
        sqlite_orm::make_column("QuarantineReason", &QuarantineMetadata::QuarantineReason),
        sqlite_orm::make_column("MalwareNameOrRule", &QuarantineMetadata::MalwareNameOrRule))));

// 로그 분석 결과에 대한 storage 타입 정의
using StorageLogAnalysisResult = decltype(sqlite_orm::make_storage("",
    sqlite_orm::make_table("LogAnalysisResult",
        sqlite_orm::make_column("ID", &LogAnalysisResult::id, sqlite_orm::primary_key().autoincrement()),
        sqlite_orm::make_column("Type", &LogAnalysisResult::type),
        sqlite_orm::make_column("Description", &LogAnalysisResult::description),
        sqlite_orm::make_column("Timestamp", &LogAnalysisResult::timestamp),
        sqlite_orm::make_column("UID", &LogAnalysisResult::uid),
        sqlite_orm::make_column("IsSuccess", &LogAnalysisResult::bIsSuccess),
        sqlite_orm::make_column("OriginalLogPath", &LogAnalysisResult::originalLogPath),
        sqlite_orm::make_column("RawLine", &LogAnalysisResult::rawLine))));


//FIM Baseline 테이블에 대한 storage 타입 정의
using StorageBaseline = decltype(sqlite_orm::make_storage("",
    sqlite_orm::make_table("baseline",
        sqlite_orm::make_column("path", &BaselineEntry::path, sqlite_orm::primary_key()),
        sqlite_orm::make_column("md5",  &BaselineEntry::md5)
    )
));

//FIM 해시값 변조 탐지 완료된 파일만 따로 모아놓는 테이블 구조
struct ModifiedEntry {
    std::string path;
    std::string current_md5;
};

//FIM 해시값 변조 탐지 테이블에 대한 storage 타입 정의
using StorageModified = decltype(sqlite_orm::make_storage("",
    sqlite_orm::make_table("modifiedhash",
        sqlite_orm::make_column("path", &ModifiedEntry::path, sqlite_orm::primary_key()),
        sqlite_orm::make_column("current_md5", &ModifiedEntry::current_md5)    
    )
));

class DBManager {
public:
    static DBManager& GetInstance();
    void InitSchema();
    DBManager(const DBManager&) = delete;
    DBManager &operator=(const DBManager&) = delete;

    // initDB
    static void InitHashDB(const std::string& dataFilePath);

    // Getter
    StorageHash& GetHashStorage();
    StorageQuarantine& GetQuarantineStorage();
    StorageLogAnalysisResult& GetLogAnalysisResultStorage();
    StorageBaseline& GetBaselineStorage();
    StorageModified& GetModifiedStorage();

private:
    DBManager();

    // storage
    StorageHash mHashStorage;
    StorageQuarantine mQuarantineStorage;
    StorageLogAnalysisResult mLogAnalysisResultStorage;
    StorageBaseline mBaselineStorage;
    StorageModified mModifiedStorage;
};