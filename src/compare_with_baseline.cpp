#include "compare_with_baseline.h"
#include "baseline_generator.h"  // compute_md5 함수 사용

#include <iostream>
#include <unordered_map>
#include <sqlite3.h>
#include <filesystem>

// baseline DB에서 path-md5 쌍을 불러오는 함수
std::unordered_map<std::string, std::string> load_baseline_hashes(const std::string& db_path) {
    std::unordered_map<std::string, std::string> baseline;
    sqlite3* db;

    //db 파일 열기
    if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK) {
        std::cerr << "[ERROR] DB 열기 실패: " << db_path << std::endl;
        return baseline;
    }

    //기준선 table 에서 모든 path, md5 쌍 조회
    const char* sql = "SELECT path, md5 FROM baseline;";
    sqlite3_stmt* stmt;

    //sql 문 준비
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "[ERROR] SELECT 쿼리 준비 실패\n";
        sqlite3_close(db);
        return baseline;
    }

    //select 결과를 한줄씩 읽어서 ㅇ=맵에 저장
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        std::string hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        baseline[path] = hash; //경로를 key, md5를 value로 저장
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return baseline;
}

// 기준선 DB의 md5와 현재 파일의 md5를 비교
void compare_with_baseline(const std::string& db_path) {
    //기준선 정보 불러옴
    std::unordered_map<std::string, std::string> baseline = load_baseline_hashes(db_path);

    //기존선에 등록된 모든 파일에 대해 반복
    for (const auto& [path, stored_hash] : baseline) {
        //파일이 실제로 존재하는지 확인
        if (!std::filesystem::exists(path)) {
            std::cout << "[WARN] 파일 없음: " << path << std::endl;
            continue;
        }

        //현재 파일의 md5 해시값 계산 (compute_md5 함수 호출함)
        std::string current_hash = BaselineGenerator::compute_md5(path);

        if (current_hash.empty()) {
            std::cout << "[ERROR] 해시 계산 실패: " << path << std::endl;
            continue;
        }

        //기준선 해시와 현재 해시 비교
        if (current_hash == stored_hash) {
            std::cout << "[OK] 일치: " << path << std::endl;
        } else {
            std::cout << "[ALERT] 변조 감지: " << path << std::endl;
            std::cout << "  - 기준선: " << stored_hash << std::endl;
            std::cout << "  - 현재값: " << current_hash << std::endl;
        }
    }
}
