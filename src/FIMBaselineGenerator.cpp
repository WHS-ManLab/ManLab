#include "FIMBaselineGenerator.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <openssl/md5.h>
#include <filesystem>
#include <set>
#include "DBManager.h"

BaselineGenerator::BaselineGenerator(const std::string& ini_path, const std::string& db_path)
    : ini_path_(ini_path), db_path_(db_path) {}

// MD5 해시 계산
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

// 경로가 제외 목록에 포함되어 있는지 확인
bool BaselineGenerator::is_excluded(const std::string& path, const std::set<std::string>& excludes) {
    for (const auto& exclude : excludes) {
        if (path == exclude || path.rfind(exclude + "/", 0) == 0) {
            return true;
        }
    }
    return false;
}

// INI 파일 파싱 및 DB 저장
void BaselineGenerator::parse_ini_and_store() {
    std::ifstream ini_file(ini_path_);
    if (!ini_file) {
        std::cerr << "[ERROR] INI 파일 열기 실패: " << ini_path_ << std::endl;
        return;
    }

    auto& storage = DBManager::GetInstance().GetBaselineStorage();

    std::set<std::string> exclude_paths;
    std::vector<std::string> target_paths;

    std::string line;
    std::string current_section;

    // 1차 파싱: 섹션별로 분류
    while (std::getline(ini_file, line)) {
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;

        // 섹션명
        if (line.front() == '[' && line.back() == ']') {
            current_section = line.substr(1, line.size() - 2);
            continue;
        }

        // 키=값 형식
        size_t eq_pos = line.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = line.substr(0, eq_pos);
            std::string value = line.substr(eq_pos + 1);
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            if (key == "Path") {
                if (current_section.rfind("EXCLUDE_", 0) == 0)
                    exclude_paths.insert(value);
                else if (current_section.rfind("TARGETS_", 0) == 0)
                    target_paths.push_back(value);
            }
        }
    }

    // 2차 처리: 경로별로 해시 생성
    for (const auto& path : target_paths) {
        std::cout << "[INI] 경로 파싱됨: " << path << std::endl;

        try {
            if (std::filesystem::is_regular_file(path)) {
                if (is_excluded(path, exclude_paths)) {
                    std::cout << "[SKIP] 제외 대상 파일: " << path << std::endl;
                    continue;
                }
                std::string hash = compute_md5(path);
                std::cout << "[단일 파일] 저장 중: " << path << " → " << hash << std::endl;
                storage.replace(BaselineEntry{path, hash});
            }
            else if (std::filesystem::is_directory(path)) {
                for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
                    if (!entry.is_regular_file()) continue;

                    std::string file_path = entry.path().string();
                    if (is_excluded(file_path, exclude_paths)) {
                        std::cout << "[SKIP] 제외 대상 파일: " << file_path << std::endl; //제대로 제외 되었는지 확인하기 위한 코드 (나중에 삭제 할 예정)
                        continue;
                    }

                    std::string hash = compute_md5(file_path);
                    std::cout << "[디렉토리 파일] 저장 중: " << file_path << " → " << hash << std::endl;
                    storage.replace(BaselineEntry{file_path, hash});
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

// 외부 호출용 함수
void BaselineGenerator::generate_and_store() {
    parse_ini_and_store();
}
