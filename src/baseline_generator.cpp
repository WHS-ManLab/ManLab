#include "baseline_generator.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <openssl/md5.h>
#include <filesystem>
#include "DBManager.h"

//ini 경로와 DB 경로를 멤버 변수로 저장
BaselineGenerator::BaselineGenerator(const std::string& ini_path, const std::string& db_path)
    : ini_path_(ini_path), db_path_(db_path) {}

//파일 경로를 받아 해당 파일의 MD5 해시값을 변환
std::string BaselineGenerator::compute_md5(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) return "";

    MD5_CTX ctx;
    MD5_Init(&ctx);

    char buffer[4096];
    //파일 끝까지 읽으면서 해시 업데이트
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

//INI 파일에서 경로를 파싱하고 해당 경로와 파일/디랙토리 해시를 DB에 저장
void BaselineGenerator::parse_ini_and_store() {
    std::ifstream ini_file(ini_path_);
    if (!ini_file) {
        std::cerr << "[ERROR] INI 파일 열기 실패: " << ini_path_ << std::endl;
        return;
    }

    auto& storage = DBManager::GetInstance().GetBaselineStorage();

    std::string line;
    while (std::getline(ini_file, line)) {
        if (line.find("Path") != std::string::npos) {
            size_t eq_pos = line.find('=');
            if (eq_pos != std::string::npos) {
                std::string path = line.substr(eq_pos + 1);
                path.erase(0, path.find_first_not_of(" \t"));
                path.erase(path.find_last_not_of(" \t") + 1);

                std::cout << "[INI] 경로 파싱됨: " << path << std::endl;

                try {
                    if (std::filesystem::is_regular_file(path)) {
                        //단일 파일인 경우 해시 계산 후 DB에 저장
                        std::string hash = compute_md5(path);
                        std::cout << "[단일 파일] 저장 중: " << path << " → " << hash << std::endl;
                        storage.replace(BaselineEntry{path, hash});
                    }
                    else if (std::filesystem::is_directory(path)) {
                        //디렉토리인 경우 재귀적으로 내부 파일들 모두 처리
                        for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
                            if (entry.is_regular_file()) {
                                std::string file_path = entry.path().string();
                                std::string hash = compute_md5(file_path);
                                std::cout << "[디렉토리 파일] 저장 중: " << file_path << " → " << hash << std::endl;
                                storage.replace(BaselineEntry{file_path, hash});
                            }
                        }
                    }
                    else {
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