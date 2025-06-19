#include "DBManager.h"
#include <unistd.h>
#include <filesystem>
#include <stdexcept>

namespace manlab {

using namespace sqlite_orm;
namespace fs = std::filesystem;

// 실행 바이너리 경로를 기준으로 db 경로를 반환하는 내부 함수
// 각 팀에서도 바이너리 경로 기준 상대 경로 반환 함수가 필요하다면 utils에 넣는 것 검토
static std::string GetDatabasePath() {
    char path[1024];
    ssize_t count = readlink("/proc/self/exe", path, sizeof(path));
    if (count == -1) {
        //로그 남기고 종료하는 로직 필요
        //로그 관련 CPP파일 생성 시 연동
    }

    fs::path exePath(path, path + count);                        // /Manlab/src/
    fs::path exeDir = exePath.parent_path();                     // /Manlab/
    fs::path dbPath = exeDir.parent_path() / "db" / "manlab.db"; // /Manlab/db/

    return dbPath.string(); // 경로 문자열 반환
}

// 내부 저장소 생성 함수
DBManager::Storage DBManager::CreateStorage() {
    const std::string dbFilePath = GetDatabasePath();

    return make_storage(dbFilePath,
        make_table("malware_hashes",
            make_column("hash_algo", &MalwareHashDB::hashAlgo),
            make_column("malware_hash", &MalwareHashDB::malwareHash),
            make_column("malware_name", &MalwareHashDB::malwareName)
        )

        // 다른 팀 테이블 이 자리에 추가
        // 헤더파일에 구조체 선언, 이 자리에 make_table과 make_column으로 쓸 테이블 선언
        // manlab.db 데이터베이스 하나에 여러 테이블이 들어가는 방식(변경 가능)

    );
}

// 전역 Storage 인스턴스를 반환하는 싱글톤 디자인 패턴
DBManager::Storage& DBManager::GetStorage() {
    static auto storage = CreateStorage();
    static bool initialized = false;
    if (!initialized) {
        storage.sync_schema();  // 테이블이 없으면 생성
        initialized = true;
    }
    return storage;
}

} // namespace manlab