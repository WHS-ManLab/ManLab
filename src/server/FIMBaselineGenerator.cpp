#include "FIMBaselineGenerator.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <openssl/md5.h>
#include <filesystem>
#include <sys/stat.h>
#include <ctime>
#include <iostream>
#include "DBManager.h"
#include "indicator.hpp"

BaselineGenerator::BaselineGenerator(const std::string& ini_path,
                                     const std::string& db_path)
    : ini_path_(ini_path), db_path_(db_path)
{}

std::string BaselineGenerator::compute_md5(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) return "";

    MD5_CTX ctx;
    MD5_Init(&ctx);

    char buffer[4096];
    while (file.read(buffer, sizeof(buffer)) || file.gcount())
        MD5_Update(&ctx, buffer, file.gcount());

    unsigned char result[MD5_DIGEST_LENGTH];
    MD5_Final(result, &ctx);

    std::ostringstream oss;
    for (int i = 0; i < MD5_DIGEST_LENGTH; ++i)
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(result[i]);
    return oss.str();
}

BaselineEntry BaselineGenerator::collect_metadata(const std::string& path,
                                                 const std::string& md5) {
    struct stat stat_buf;
    if (stat(path.c_str(), &stat_buf) != 0)
        throw std::runtime_error("stat() 실패: " + path);

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

void BaselineGenerator::parse_ini_and_store(std::ostream& out) {
    std::ifstream ini_file(ini_path_);
    if (!ini_file) {
        out << "[ERROR] INI 파일 열기 실패: " << ini_path_ << "\n";
        return;
    }

    auto& storage = DBManager::GetInstance().GetBaselineStorage();
    std::vector<std::string> target_paths;
    std::string line;
    while (std::getline(ini_file, line)) {
        if (line.find("Path") != std::string::npos) {
            auto eq = line.find('=');
            if (eq != std::string::npos) {
                std::string p = line.substr(eq + 1);
                p.erase(0, p.find_first_not_of(" \t"));
                p.erase(p.find_last_not_of(" \t") + 1);
                target_paths.push_back(p);
            }
        }
    }

    size_t total_files = 0;
    for (auto& p : target_paths) {
        if (std::filesystem::is_regular_file(p)) ++total_files;
        else if (std::filesystem::is_directory(p)) {
            for (auto& e : std::filesystem::recursive_directory_iterator(p))
                if (e.is_regular_file()) ++total_files;
        }
    }
    if (total_files == 0) {
        out << "[INFO] 대상 파일이 없습니다.\n";
        return;
    }

    // 1) ProgressBar 생성 (CompletedText 비활성화)
    indicators::ProgressBar bar(
        indicators::option::BarWidth{50},
        indicators::option::Start{"["},
        indicators::option::Fill{"="},
        indicators::option::Lead{">"},
        indicators::option::Remainder{"-"},
        indicators::option::End{"]"},
        indicators::option::ShowPercentage{true},
        indicators::option::ShowElapsedTime{true},
        indicators::option::ShowRemainingTime{true},
        indicators::option::Stream{out});

    // 2) 첫 번째 set_progress 호출로 바를 그린 뒤,
    bar.set_progress(0);

    // 3) 바로 다음 줄로 내려와서, “파일 경로”가 찍힐 위치를 저장
    out << "\n";
    out << "\033[s";  // 이 시점부터 아래가 파일 출력 위치

    size_t processed = 0;
    for (auto& p : target_paths) {
        try {
            if (std::filesystem::is_regular_file(p)) {
                auto h = compute_md5(p);
                storage.replace(collect_metadata(p, h));
                // 4) 바를 업데이트하기 전에, 위쪽(바 위치)으로 커서 복원
                out << "\033[u"    // 저장된 위치 복원(파일 위치)
                    << "\033[1A"   // 한 줄 위로 → 바가 있던 라인
                    << "\r";       // 캐리지 리턴으로 라인 덮어쓰기
                bar.set_progress(++processed * 100.0f / total_files);

                // 5) 다시 파일 출력 위치로 복원 → 클리어 → 파일 경로 출력
                out << "\033[1B"  // 한 줄 아래로 (파일 위치)
                    <<"\r"
                    << "\033[2K"  // 줄 전체 클리어
                    << p << "\n";
            }
            else if (std::filesystem::is_directory(p)) {
                for (auto& e : std::filesystem::recursive_directory_iterator(p)) {
                    if (!e.is_regular_file()) continue;
                    auto fp = e.path().string();
                    auto h  = compute_md5(fp);
                    storage.replace(collect_metadata(fp, h));

                    out << "\033[u"
                        << "\033[1A"
                        << "\r";
                    bar.set_progress(++processed * 100.0f / total_files);
                    out << "\033[1B"
                        << "\r"
                        << "\033[2K" 
                        << fp << "\n";
                }
            }
        }
        catch (const std::exception& ex) {
            out << "[ERROR] 처리 중 예외 발생: " << ex.what() << "\n";
        }
    }

    // 6) 완료 메시지는 그대로 파일 위치 아래에 한 번만
    out << "[INFO] Baseline 생성 완료.\n";
}

void BaselineGenerator::generate_and_store(std::ostream& out) {
    parse_ini_and_store(out);
}
