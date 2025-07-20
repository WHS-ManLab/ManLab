#include "DBManager.h"
#include "Paths.h"
#include <fstream>  // 파일 읽기용
#include <sstream>  // 문자열 파싱용
#include <filesystem> // 파일 경로 확인용
#include <iostream> //디버그용

namespace fs = std::filesystem;

DBManager& DBManager::GetInstance()
{
    static DBManager sInstance;
    return sInstance;
}

// 생성자
// SQLite DB 파일을 열고 ORM 매핑 정보를 담은 객체를 생성
// SQLite 파일 열기 + 테이블 명과 컬럼 정의를 메모리에 등록
// 싱글톤 구조이기에 각 프로세스마다 오직 한 번만 생성
DBManager::DBManager()
    : mHashStorage(sqlite_orm::make_storage(
        PATH_HASH_DB,
            sqlite_orm::make_table("MalwareHashDB",
            sqlite_orm::make_column("Hash", &MalwareHashDB::Hash),
            sqlite_orm::make_column("Algorithm", &MalwareHashDB::Algorithm),
            sqlite_orm::make_column("MalwareName", &MalwareHashDB::MalwareName),
            sqlite_orm::make_column("SecurityVendors", &MalwareHashDB::SecurityVendors),
            sqlite_orm::make_column("HashLicense", &MalwareHashDB::HashLicense)))),

      mQuarantineStorage(sqlite_orm::make_storage(
        PATH_QUARANTINE_DB,
            sqlite_orm::make_table("QuarantineMetadata",
            sqlite_orm::make_column("OriginalPath", &QuarantineMetadata::OriginalPath),
            sqlite_orm::make_column("QuarantinedFileName", &QuarantineMetadata::QuarantinedFileName),
            sqlite_orm::make_column("OriginalSize", &QuarantineMetadata::OriginalSize),
            sqlite_orm::make_column("QuarantineDate", &QuarantineMetadata::QuarantineDate),
            sqlite_orm::make_column("QuarantineReason", &QuarantineMetadata::QuarantineReason),
            sqlite_orm::make_column("MalwareNameOrRule", &QuarantineMetadata::MalwareNameOrRule)))),

      mLogAnalysisResultStorage(sqlite_orm::make_storage(
        PATH_LOG_ANALYSIS_DB,
            sqlite_orm::make_table("LogAnalysisResult",
            sqlite_orm::make_column("ID", &LogAnalysisResult::id, sqlite_orm::primary_key().autoincrement()),
            sqlite_orm::make_column("Type", &LogAnalysisResult::type),
            sqlite_orm::make_column("Description", &LogAnalysisResult::description),
            sqlite_orm::make_column("Timestamp", &LogAnalysisResult::timestamp),
            sqlite_orm::make_column("UID", &LogAnalysisResult::uid),
            sqlite_orm::make_column("IsSuccess", &LogAnalysisResult::bIsSuccess),
            sqlite_orm::make_column("OriginalLogPath", &LogAnalysisResult::originalLogPath),
            sqlite_orm::make_column("RawLine", &LogAnalysisResult::rawLine)))),
                                 
       mBaselineStorage(sqlite_orm::make_storage(
        PATH_BASELINE_DB,
            sqlite_orm::make_table("baseline",
            sqlite_orm::make_column("path", &BaselineEntry::path, sqlite_orm::primary_key()),
            sqlite_orm::make_column("md5",  &BaselineEntry::md5),
            sqlite_orm::make_column("permission",  &BaselineEntry::permission),
            sqlite_orm::make_column("uid",         &BaselineEntry::uid),
            sqlite_orm::make_column("gid",         &BaselineEntry::gid),
            sqlite_orm::make_column("ctime",       &BaselineEntry::ctime),
            sqlite_orm::make_column("mtime",       &BaselineEntry::mtime),
            sqlite_orm::make_column("size",        &BaselineEntry::size)))),

      mRealTimeMonitorStorage(sqlite_orm::make_storage(
        PATH_REAL_TIME_MONITOR_DB,
        sqlite_orm::make_table("RealTimeMonitor",
        sqlite_orm::make_column("ID", &RealtimeEventLog::id, sqlite_orm::primary_key().autoincrement()),
        sqlite_orm::make_column("PATH", &RealtimeEventLog::path),
        sqlite_orm::make_column("EVENT_TYPE", &RealtimeEventLog::eventType),
        sqlite_orm::make_column("TIMESTAMP", &RealtimeEventLog::timestamp)
        )
    )),

      mModifiedStorage(sqlite_orm::make_storage(
        PATH_MODIFIED_DB,
            sqlite_orm::make_table("modifiedhash",
            sqlite_orm::make_column("path", &ModifiedEntry::path, sqlite_orm::primary_key()),
            sqlite_orm::make_column("current_md5", &ModifiedEntry::current_md5)))),

     mScanReportStorage(sqlite_orm::make_storage(
        PATH_SCAN_REPORT_DB,
            sqlite_orm::make_table("ScanReport",
            sqlite_orm::make_column("id", &ScanReport::id, sqlite_orm::primary_key().autoincrement()),
            sqlite_orm::make_column("Type", &ScanReport::type),
            sqlite_orm::make_column("Date", &ScanReport::date),
            sqlite_orm::make_column("Report", &ScanReport::report),
            sqlite_orm::make_column("Detected", &ScanReport::detected))
        
            //리포트 관련 테이블 생성
            ))
{}

// 데이터베이스를 생성하는 로직
// init시 오직 한 번만 호출
void DBManager::InitSchema() 
{
    mHashStorage.sync_schema();
    mQuarantineStorage.sync_schema();
    mLogAnalysisResultStorage.sync_schema();
    mBaselineStorage.sync_schema();
    mRealTimeMonitorStorage.sync_schema();
    mModifiedStorage.sync_schema();
    mScanReportStorage.sync_schema();
}

// malware_hashes.txt 파일에서 해시 데이터를 읽어 DB에 삽입하는 함수
void DBManager::InitHashDB() 
{
    StorageHash& storage = DBManager::GetInstance().GetHashStorage();

    // 파일이 존재하지 않으면 함수 종료
    if (!fs::exists(PATH_HASH_INIT_TXT)) 
    {
        //TODO : 에러 로그 입력
        return;
    }

    std::ifstream dataFile(PATH_HASH_INIT_TXT);
    if (!dataFile.is_open()) 
    {
        return;
    }

    std::string line;
    // 헤더 줄 건너뛰기
    std::getline(dataFile, line); 

    while (std::getline(dataFile, line)) 
    {
        if (line.empty()) 
        {
            continue; // 빈 줄 건너뛰기
        }
        std::istringstream iss(line);
        MalwareHashDB entry;

        // 탭으로 구분된 값을 읽어 MalwareHashDB 구조체에 저장
        if (std::getline(iss, entry.Hash, '\t') &&
            std::getline(iss, entry.Algorithm, '\t') &&
            std::getline(iss, entry.MalwareName, '\t') &&
            std::getline(iss, entry.SecurityVendors, '\t') &&
            std::getline(iss, entry.HashLicense, '\t'))
        {
            try 
            {
                // 이미 존재하는 해시는 무시
                storage.insert(entry);
            } 
            catch (const std::exception& e) 
            {
                //TODO : 에러 로그 입력
            }
        } 
    }
}

StorageHash& DBManager::GetHashStorage()
{
    return mHashStorage;
}

StorageQuarantine& DBManager::GetQuarantineStorage()
{
    return mQuarantineStorage;
}

StorageLogAnalysisResult& DBManager::GetLogAnalysisResultStorage()
{
    return mLogAnalysisResultStorage;
}
  
StorageBaseline& DBManager::GetBaselineStorage() 
{
    return mBaselineStorage;
}

StorageRealTimeMonitor& DBManager::GetRealTimeMonitorStorage() 
{
    return mRealTimeMonitorStorage;
}

StorageModified& DBManager::GetModifiedStorage() 
{
    return mModifiedStorage;
}

StorageScanReport& DBManager::GetScanReportStorage()
{
    return mScanReportStorage;
}