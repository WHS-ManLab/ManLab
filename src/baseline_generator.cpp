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

void BaselineGenerator::parse_ini_and_store() {
    std::ifstream ini_file(ini_path_);
    if (!ini_file) {
        std::cerr << "INI 파일 열기 실패: " << ini_path_ << std::endl;
        return;
    }

    std::string line;
    while (std::getline(ini_file, line)) {
        if (line.find("PATH=") == 0) {
            std::string path = line.substr(5);
            for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
                if (entry.is_regular_file()) {
                    std::string file_path = entry.path().string();
                    std::string hash = compute_md5(file_path);

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
        }
    }
}

void BaselineGenerator::generate_and_store() {
    parse_ini_and_store();
}
