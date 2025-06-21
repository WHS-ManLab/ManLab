#include "baseline_generator.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <openssl/md5.h>
#include <sqlite3.h>
#include <filesystem>

BaselineGenerator::BaselineGenerator(const std::string& ini_path, const std::string& db_path)
    : ini_path_(ini_path), db_path_(db_path) {}

// 특정 파일의 MD5 해시 값을 계산하는 함수
std::string BaselineGenerator::compute_md5(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) return "";

    MD5_CTX ctx;
    MD5_Init(&ctx);

    char buffer[4096];
    while (file.read(buffer, sizeof(buffer)) || file.gcount()) {
        MD5_Update(&ctx, buffer, file.gcount());
    }

    unsigned char result[MD5_DIGEST_LENGTH];
    MD5_Final(result, &ctx);

    std::ostringstream oss;
    for (int i = 0; i < MD5_DIGEST_LENGTH; ++i)
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)result[i];

    return oss.str();
}

//ini 파일을 읽고 해당 경로 내 파일들의 MD5 해시를 계산해 DB에 저장하는 함수
void BaselineGenerator::parse_ini_and_store() {
    std::ifstream ini_file(ini_path_);
    if (!ini_file) {
        std::cerr << "[ERROR] INI 파일 열기 실패: " << ini_path_ << std::endl;
        return;
    }

    std::string line;
    while (std::getline(ini_file, line)) {
        // "Path" 키가 있는 라인만 처리
        if (line.find("Path") != std::string::npos) {
            size_t eq_pos = line.find('=');
            if (eq_pos != std::string::npos) {
                std::string path = line.substr(eq_pos + 1);
                path.erase(0, path.find_first_not_of(" \t"));
                path.erase(path.find_last_not_of(" \t") + 1);

                std::cout << "[INI] 경로 파싱됨: " << path << std::endl;

                try {
                    // (1) 단일 파일인 경우
                    if (std::filesystem::is_regular_file(path)) {
                        std::string hash = compute_md5(path);
                        std::cout << "[단일 파일] 저장 중: " << path << " → " << hash << std::endl;

                        sqlite3* db;
                        sqlite3_open(db_path_.c_str(), &db);
                        const char* create_sql = "CREATE TABLE IF NOT EXISTS baseline (path TEXT PRIMARY KEY, md5 TEXT);";
                        sqlite3_exec(db, create_sql, nullptr, nullptr, nullptr);

                        std::string insert_sql = "INSERT OR REPLACE INTO baseline (path, md5) VALUES (?, ?);";
                        sqlite3_stmt* stmt;
                        sqlite3_prepare_v2(db, insert_sql.c_str(), -1, &stmt, nullptr);
                        sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_STATIC);
                        sqlite3_bind_text(stmt, 2, hash.c_str(), -1, SQLITE_STATIC);
                        sqlite3_step(stmt);
                        sqlite3_finalize(stmt);
                        sqlite3_close(db);
                    }
                    // (2) 디렉토리인 경우
                    else if (std::filesystem::is_directory(path)) {
                        for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
                            if (entry.is_regular_file()) {
                                std::string file_path = entry.path().string();
                                std::string hash = compute_md5(file_path);
                                std::cout << "[디렉토리 파일] 저장 중: " << file_path << " → " << hash << std::endl;

                                sqlite3* db;
                                sqlite3_open(db_path_.c_str(), &db);
                                const char* create_sql = "CREATE TABLE IF NOT EXISTS baseline (path TEXT PRIMARY KEY, md5 TEXT);";
                                sqlite3_exec(db, create_sql, nullptr, nullptr, nullptr);

                                std::string insert_sql = "INSERT OR REPLACE INTO baseline (path, md5) VALUES (?, ?);";
                                sqlite3_stmt* stmt;
                                sqlite3_prepare_v2(db, insert_sql.c_str(), -1, &stmt, nullptr);
                                sqlite3_bind_text(stmt, 1, file_path.c_str(), -1, SQLITE_STATIC);
                                sqlite3_bind_text(stmt, 2, hash.c_str(), -1, SQLITE_STATIC);
                                sqlite3_step(stmt);
                                sqlite3_finalize(stmt);
                                sqlite3_close(db);
                            }
                        }
                    } else {
                        std::cerr << "[ERROR] 유효하지 않은 경로입니다: " << path << std::endl;
                    }

                } catch (const std::filesystem::filesystem_error& e) {
                    std::cerr << "[ERROR] 파일/디렉토리 접근 실패: " << e.what() << std::endl;
                }
            }
        }
    }
}


void BaselineGenerator::generate_and_store() {
    parse_ini_and_store();
}