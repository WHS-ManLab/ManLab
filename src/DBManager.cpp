#include "DBManager.h"
#include <fstream>  // 파일 읽기용
#include <sstream>  // 문자열 파싱용
#include <filesystem> // 파일 경로 확인용
#include <iostream> //디버그용

DBManager& DBManager::GetInstance()
{
    static DBManager sInstance;
    return sInstance;
}

DBManager::DBManager()
    : mHashStorage(sqlite_orm::make_storage(
          "/ManLab/db/hash.db",
          sqlite_orm::make_table("MalwareHashDB",
            sqlite_orm::make_column("Hash", &MalwareHashDB::Hash),
            sqlite_orm::make_column("Algorithm", &MalwareHashDB::Algorithm),
            sqlite_orm::make_column("MalwareName", &MalwareHashDB::MalwareName),
            sqlite_orm::make_column("SecurityVendors", &MalwareHashDB::SecurityVendors),
            sqlite_orm::make_column("HashLicense", &MalwareHashDB::HashLicense)))),

      mQuarantineStorage(sqlite_orm::make_storage(
          "/ManLab/db/quarantine.db",
          sqlite_orm::make_table("QuarantineMetadata",
                                 sqlite_orm::make_column("OriginalPath", &QuarantineMetadata::OriginalPath),
                                 sqlite_orm::make_column("QuarantinedFileName", &QuarantineMetadata::QuarantinedFileName),
                                 sqlite_orm::make_column("OriginalSize", &QuarantineMetadata::OriginalSize),
                                 sqlite_orm::make_column("QuarantineDate", &QuarantineMetadata::QuarantineDate),
                                 sqlite_orm::make_column("QuarantineReason", &QuarantineMetadata::QuarantineReason),
                                 sqlite_orm::make_column("MalwareNameOrRule", &QuarantineMetadata::MalwareNameOrRule)))),

      mLogAnalysisResultStorage(sqlite_orm::make_storage(
          "/ManLab/db/logAnalysisResult.db",
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
        "/ManLab/db/baseline.db",
        sqlite_orm::make_table("baseline",
            sqlite_orm::make_column("path", &BaselineEntry::path, sqlite_orm::primary_key()),
            sqlite_orm::make_column("md5",  &BaselineEntry::md5))))
{}

void DBManager::InitSchema() {
    mHashStorage.sync_schema();
    mQuarantineStorage.sync_schema();
    mLogAnalysisResultStorage.sync_schema();
    mBaselineStorage.sync_schema();
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