#include "DBManager.h"
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

// malware_hashes.txt 파일에서 해시 데이터를 읽어 DB에 삽입하는 함수
void DBManager::InitHashDB(const std::string& dataFilePath) {
    StorageHash& storage = DBManager::GetInstance().GetHashStorage();

    // 파일이 존재하지 않으면 함수 종료
    if (!fs::exists(dataFilePath)) {
        return;
    }

    std::ifstream dataFile(dataFilePath);
    if (!dataFile.is_open()) {
        return;
    }

    std::string line;
    // 헤더 줄 건너뛰기
    std::getline(dataFile, line); 

    while (std::getline(dataFile, line)) {
        if (line.empty()) {
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
            try {
                // 이미 존재하는 해시는 무시
                storage.insert(entry);
            } catch (const std::exception& e) {}
        } else {}
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