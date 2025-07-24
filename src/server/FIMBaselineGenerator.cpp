#include "FIMBaselineGenerator.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <openssl/md5.h>
#include <filesystem>
#include <sys/stat.h>
#include <ctime>
#include <iostream>
#include <unordered_set>
#include "DBManager.h"
#include "indicator.hpp"

//ini 경로와 DB 경로를 멤버 변수로 저장
BaselineGenerator::BaselineGenerator(const std::string& ini_path,
                                     const std::string& db_path)
    : ini_path_(ini_path), db_path_(db_path)
{}

//파일 경로를 받아 해당 파일의 MD5 해시값을 변환
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

//파일의 메타데이터 수집
BaselineEntry BaselineGenerator::collect_metadata(const std::string& path,
                                                 const std::string& md5) {
    struct stat stat_buf;
    if (stat(path.c_str(), &stat_buf) != 0)
        throw std::runtime_error("stat() 실패: " + path);

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
        out << "[ERROR] INI 파일 열기 실패: " << ini_path_ << "\n";
        return;
    }

    auto& storage = DBManager::GetInstance().GetBaselineStorage();

    DBManager::GetInstance().GetModifiedStorage().remove_all<ModifiedEntry>();

    std::vector<std::string> target_paths;
    std::unordered_set<std::string> exclude_paths;

    std::string line;
    bool is_exclude = false;

    while (std::getline(ini_file, line)) {
        if (line.find("[EXCLUDES") != std::string::npos) {
            is_exclude = true;
            continue;
        }
        if (line.find("[TARGETS") != std::string::npos) {
            is_exclude = false;
            continue;
        }
        if (line.find("Path") != std::string::npos) {
            auto eq = line.find('=');
            if (eq != std::string::npos) {
                std::string p = line.substr(eq + 1);
                p.erase(0, p.find_first_not_of(" \t"));
                p.erase(p.find_last_not_of(" \t") + 1);

                if (is_exclude)
                    exclude_paths.insert(p);
                else
                    target_paths.push_back(p);
            }
        }
    }

    // 전체 파일 개수 계산
    size_t total_files = 0;
    for (auto& p : target_paths) {
        if (std::filesystem::is_regular_file(p)) {
            bool excluded = false;
            for (const auto& ex : exclude_paths)
                if (p.rfind(ex, 0) == 0) { excluded = true; break; }
            if (!excluded) ++total_files;
        }
        else if (std::filesystem::is_directory(p)) {
            for (auto& e : std::filesystem::recursive_directory_iterator(p)) {
                if (!e.is_regular_file()) continue;
                std::string fp = e.path().string();
                bool excluded = false;
                for (const auto& ex : exclude_paths)
                    if (fp.rfind(ex, 0) == 0) { excluded = true; break; }
                if (!excluded) ++total_files;
            }
        }
    }

    if (total_files == 0) {
        out << "[INFO] 대상 파일이 없습니다.\n";
        return;
    }

    //ProgressBar 생성 
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

    bar.set_progress(0);

    out << "\n";
    out << "\033[s";  

    size_t processed = 0;
    for (auto& p : target_paths) {
        try {
            if (std::filesystem::is_regular_file(p)) {
                bool excluded = false;
                for (const auto& ex : exclude_paths)
                    if (p.rfind(ex, 0) == 0) { excluded = true; break; }
                if (excluded) continue;

                auto h = compute_md5(p);
                storage.replace(collect_metadata(p, h));
                out << "\033[u"    
                    << "\033[1A"   
                    << "\r";       

                bar.set_progress(++processed * 100.0f / total_files);

                out << "\033[1B"  
                    <<"\r"
                    << "\033[2K"  
                    << p << "\n";
            }
            else if (std::filesystem::is_directory(p)) {
                for (auto& e : std::filesystem::recursive_directory_iterator(p)) {
                    if (!e.is_regular_file()) continue;
                    std::string fp = e.path().string();

                    bool excluded = false;
                    for (const auto& ex : exclude_paths)
                        if (fp.rfind(ex, 0) == 0) { excluded = true; break; }
                    if (excluded) continue;

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
    
    //파일 경로 삭제
    out << "\033[1A"
            << "\033[2K"
            << "\r";

    //완료 후 요약 정보 출력
    out << "\n[SUCCESS] Baseline 생성 완료\n";
    out << "\n";
    out << "총 검사 파일 수: " << processed << "\n";
    out << "\n";

    out << "대상 경로:\n";
    for (const auto& tp : target_paths)
        out << "  - " << tp << "\n";
        out << "\n";
        
    out << "제외 경로:\n";
    for (const auto& ep : exclude_paths)
        out << "  - " << ep << "\n";

}

void BaselineGenerator::generate_and_store(std::ostream& out) {
    parse_ini_and_store(out);
}
