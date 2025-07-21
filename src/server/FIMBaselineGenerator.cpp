#include "FIMBaselineGenerator.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <openssl/md5.h>
#include <filesystem>
#include <sys/stat.h>
#include <ctime>
#include "DBManager.h"
#include "indicator.hpp"

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

//파일의 메타데이터 수집
BaselineEntry BaselineGenerator::collect_metadata(const std::string& path, const std::string& md5) {
    struct stat stat_buf;
    if (stat(path.c_str(), &stat_buf) != 0) {
        throw std::runtime_error("stat() 실패: " + path);
    }

    //권한 문자열 생성
    std::stringstream perm_ss;
    perm_ss << ((stat_buf.st_mode & S_IRUSR) ? "r" : "-")
            << ((stat_buf.st_mode & S_IWUSR) ? "w" : "-")
            << ((stat_buf.st_mode & S_IXUSR) ? "x" : "-")
            << ((stat_buf.st_mode & S_IRGRP) ? "r" : "-")
            << ((stat_buf.st_mode & S_IWGRP) ? "w" : "-")
            << ((stat_buf.st_mode & S_IXGRP) ? "x" : "-")
            << ((stat_buf.st_mode & S_IROTH) ? "r" : "-")
            << ((stat_buf.st_mode & S_IWOTH) ? "w" : "-")
            << ((stat_buf.st_mode & S_IXOTH) ? "x" : "-");

    auto format_time = [](time_t t) {
        std::tm* tm_ptr = std::localtime(&t);
        std::ostringstream oss;
        oss << std::put_time(tm_ptr, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    };

    //모든 정보를 BaselineEntry 구조체에 담아 반환
    return BaselineEntry{
        path,
        md5,
        perm_ss.str(),
        static_cast<int>(stat_buf.st_uid),
        static_cast<int>(stat_buf.st_gid),
        format_time(stat_buf.st_ctime),
        format_time(stat_buf.st_mtime),
        static_cast<uintmax_t>(stat_buf.st_size)
    };
}

//INI 파일에서 경로를 파싱하고 해당 경로와 파일/디랙토리 해시를 DB에 저장
void BaselineGenerator::parse_ini_and_store(std::ostream& out) {
    std::ifstream ini_file(ini_path_);
    if (!ini_file) {
        out << "[ERROR] INI 파일 열기 실패: " << ini_path_ << std::endl;
        return;
    }

    auto& storage = DBManager::GetInstance().GetBaselineStorage();
    std::vector<std::string> target_paths;
    std::string line;

    while (std::getline(ini_file, line)) {
        if (line.find("Path") != std::string::npos) {
            size_t eq_pos = line.find('=');
            if (eq_pos != std::string::npos) {
                std::string path = line.substr(eq_pos + 1);
                path.erase(0, path.find_first_not_of(" \t"));
                path.erase(path.find_last_not_of(" \t") + 1);
                target_paths.push_back(path);
            }
        }
    }

    // 전체 파일 개수 계산
    size_t total_files = 0;
    for (const auto& path : target_paths) {
        if (std::filesystem::is_regular_file(path)) {
            ++total_files;
        } else if (std::filesystem::is_directory(path)) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
                if (entry.is_regular_file()) ++total_files;
            }
        }
    }

    if (total_files == 0) {
        out << "[INFO] 대상 파일이 없습니다.\n";
        return;
    }

    //프로그래스바 구현
    indicators::ProgressBar bar{
        indicators::option::BarWidth{50},
        indicators::option::Start{"["},
        indicators::option::Fill{"="},
        indicators::option::Lead{">"},
        indicators::option::Remainder{"-"},
        indicators::option::End{"]"},
        indicators::option::ShowPercentage{true},
        indicators::option::ShowElapsedTime{true},
        indicators::option::ShowRemainingTime{true},
        indicators::option::Stream{out}
    };

    size_t processed = 0;

    for (const auto& path : target_paths) {
        try {
            if (std::filesystem::is_regular_file(path)) {
                std::string hash = compute_md5(path);
                auto entry = collect_metadata(path, hash);
                storage.replace(entry);
                bar.set_progress(++processed * 100.0f / total_files);
            } else if (std::filesystem::is_directory(path)) {
                for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
                    if (!entry.is_regular_file()) continue;
                    std::string file_path = entry.path().string();
                    std::string hash = compute_md5(file_path);
                    auto meta_entry = collect_metadata(file_path, hash);
                    storage.replace(meta_entry);
                    bar.set_progress(++processed * 100.0f / total_files);
                }
            }
        } catch (const std::exception& e) {
            out << "[ERROR] 처리 중 예외 발생: " << e.what() << std::endl;
        }
    }

    out << "\n[INFO] Baseline 생성 완료.\n";
}

void BaselineGenerator::generate_and_store(std::ostream& out) {
    parse_ini_and_store(out);
}
