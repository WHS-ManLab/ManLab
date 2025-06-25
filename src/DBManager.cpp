#include "DBManager.h"
#include <iostream> //디버그용

DBManager& DBManager::GetInstance() {
    static DBManager instance;
    return instance;
}

DBManager::DBManager()
    : mHashStorage(sqlite_orm::make_storage(
        "/ManLab/db/hash.db",
        sqlite_orm::make_table("MalwareHashDB",
            sqlite_orm::make_column("hashAlgo", &MalwareHashDB::hashAlgo),
            sqlite_orm::make_column("malwareHash", &MalwareHashDB::malwareHash),
            sqlite_orm::make_column("malwareName", &MalwareHashDB::malwareName)
        )
    )),

      mQuarantineStorage(sqlite_orm::make_storage(
        "/ManLab/db/quarantine.db",
        sqlite_orm::make_table("QuarantineMetadata",
            sqlite_orm::make_column("OriginalPath", &QuarantineMetadata::OriginalPath),
            sqlite_orm::make_column("QuarantinedFileName", &QuarantineMetadata::QuarantinedFileName),
            sqlite_orm::make_column("OriginalSize", &QuarantineMetadata::OriginalSize),
            sqlite_orm::make_column("QuarantineDate", &QuarantineMetadata::QuarantineDate),
            sqlite_orm::make_column("QuarantineReason", &QuarantineMetadata::QuarantineReason),
            sqlite_orm::make_column("MalwareNameOrRule", &QuarantineMetadata::MalwareNameOrRule)
        )
    )),
        mBaselineStorage(sqlite_orm::make_storage(
        "/ManLab/db/baseline.db",
        sqlite_orm::make_table("baseline",
            sqlite_orm::make_column("path", &BaselineEntry::path, sqlite_orm::primary_key()),
            sqlite_orm::make_column("md5",  &BaselineEntry::md5)
        )
    ))

{}

void DBManager::InitSchema() {
    mHashStorage.sync_schema();
    mQuarantineStorage.sync_schema();
    mBaselineStorage.sync_schema();
}

StorageHash& DBManager::GetHashStorage() {
    return mHashStorage;
}

StorageQuarantine& DBManager::GetQuarantineStorage() {
    return mQuarantineStorage;
}

StorageBaseline& DBManager::GetBaselineStorage() {
    return mBaselineStorage;
}