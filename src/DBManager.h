#pragma once

#include <string>
#include <sqlite_orm/sqlite_orm.h>

namespace manlab {

struct MalwareHashDB {  
    std::string hashAlgo;
    std::string malwareHash;
    std::string malwareName;
}; // 악성코드 해시 값 데이터베이스 구조체 

    // 이곳에 각 팀별 데이터베이스 구조체 선언
    // 구조체 이름 : XXXDB

class DBManager {
public:
    using Storage = decltype(CreateStorage());

    static Storage& GetStorage();

private:
    static Storage CreateStorage();
    string GetDatabasePath();
};

}  // namespace manlab