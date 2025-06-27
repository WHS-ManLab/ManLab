#pragma once
#include <sqlite_orm.h> 
#include <string>

/* 
============================================
자신의 팀 전용 테이블과 DB를 다음 순서로 정의하세요.

순서:
1. 팀에서 사용할 데이터 구조 정의 (DBManager.h)
2. 각각의 DB 파일에 대한 storage 타입 정의(DBManager.h)
3. GETTER와 Storege 타입 변수 정의(DBManager.h)
4. 생성자 초기화 리스트에 make_storage() 추가 (DBManager.cpp)
5. storage.sync_schema()호출 추가 (DBManager.cpp)
============================================
*/


// 악성코드 해시 DB 테이블 구조
struct MalwareHashDB {
    std::string hashAlgo;
    std::string malwareHash;
    std::string malwareName;
};

// 격리 메타데이터 DB 테이블 구조
struct QuarantineMetadata {
    std::string OriginalPath;
    std::string QuarantinedFileName;
    long long OriginalSize;
    std::string QuarantineDate;
    std::string QuarantineReason;
    std::string MalwareNameOrRule;
};

// 악성코드 해시에 대한 stroage타입 정의
using StorageHash = decltype(sqlite_orm::make_storage("",
    sqlite_orm::make_table("MalwareHashDB",
        sqlite_orm::make_column("hashAlgo", &MalwareHashDB::hashAlgo),
        sqlite_orm::make_column("malwareHash", &MalwareHashDB::malwareHash),
        sqlite_orm::make_column("malwareName", &MalwareHashDB::malwareName)
    )
));

// 악성코드 격리 메타데이터에 대한 stroage타입 정의
using StorageQuarantine = decltype(sqlite_orm::make_storage("",
    sqlite_orm::make_table("QuarantineMetadata",
        sqlite_orm::make_column("OriginalPath", &QuarantineMetadata::OriginalPath),
        sqlite_orm::make_column("QuarantinedFileName", &QuarantineMetadata::QuarantinedFileName),
        sqlite_orm::make_column("OriginalSize", &QuarantineMetadata::OriginalSize),
        sqlite_orm::make_column("QuarantineDate", &QuarantineMetadata::QuarantineDate),
        sqlite_orm::make_column("QuarantineReason", &QuarantineMetadata::QuarantineReason),
        sqlite_orm::make_column("MalwareNameOrRule", &QuarantineMetadata::MalwareNameOrRule)
    )
));

//FIM Baseline 테이블 구조
struct BaselineEntry {
    std::string path;
    std::string md5;
};

//FIM Baseline 테이블에 대한 storage 타입 정의
using StorageBaseline = decltype(sqlite_orm::make_storage("",
    sqlite_orm::make_table("baseline",
        sqlite_orm::make_column("path", &BaselineEntry::path, sqlite_orm::primary_key()),
        sqlite_orm::make_column("md5",  &BaselineEntry::md5)
    )
));

class DBManager {
public:
    static DBManager& GetInstance();
    void InitSchema();
    DBManager(const DBManager&) = delete;
    DBManager& operator=(const DBManager&) = delete;

    //Getter
    StorageHash& GetHashStorage();
    StorageQuarantine& GetQuarantineStorage();
    StorageBaseline& GetBaselineStorage();


private:
    DBManager();

    // storage
    StorageHash mHashStorage;
    StorageQuarantine mQuarantineStorage;
    StorageBaseline mBaselineStorage;
};