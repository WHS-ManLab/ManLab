#include "FIMBaselineGenerator.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <openssl/md5.h>
#include <filesystem>
#include <set>
#include <sys/stat.h>
#include <ctime>
#include "DBManager.h"
#include "indicator.hpp"

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

BaselineEntry BaselineGenerator::collect_metadata(const std::string& path, const std::string& md5) {
    struct stat stat_buf;
    if (stat(path.c_str(), &stat_buf) != 0) {
        throw std::runtime_error("stat() 실패: " + path);
    }

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

bool BaselineGenerator::is_excluded(const std::string& path, const std::set<std::string>& excludes) {
    for (const auto& exclude : excludes) {
        if (path == exclude || path.rfind(exclude + "/", 0) == 0) {
            return true;
        }
    }
    return false;
}

void BaselineGenerator::parse_ini_and_store() {
    std::ifstream ini_file(ini_path_);
    if (!ini_file) {
        std::cerr << "[ERROR] INI 파일 열기 실패: " << ini_path_ << std::endl;
        return;
    }

    auto& storage = DBManager::GetInstance().GetBaselineStorage();
    std::set<std::string> exclude_paths;
    std::vector<std::string> target_paths;
    std::string line, current_section;

    while (std::getline(ini_file, line)) {
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line.front() == '[' && line.back() == ']') {
            current_section = line.substr(1, line.size() - 2);
            continue;
        }

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

    indicators::show_console_cursor(false);
    size_t total_files = 0;
    for (const auto& path : target_paths) {
        if (std::filesystem::is_directory(path)) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
                if (entry.is_regular_file()) ++total_files;
            }
        } else if (std::filesystem::is_regular_file(path)) {
            ++total_files;
        }
    }

    indicators::ProgressBar bar{
        indicators::option::BarWidth{50},
        indicators::option::Start{"["},
        indicators::option::Fill{"■"},
        indicators::option::Lead{">"},
        indicators::option::Remainder{"-"},
        indicators::option::End{"]"},
        indicators::option::ForegroundColor{indicators::Color::yellow},
        indicators::option::ShowPercentage{true},
        indicators::option::ShowElapsedTime{true},
        indicators::option::ShowRemainingTime{true},
        indicators::option::Stream{std::cerr}
    };

    size_t current = 0;

    for (const auto& path : target_paths) {
        try {
            if (std::filesystem::is_regular_file(path)) {
                if (is_excluded(path, exclude_paths)) continue;
                auto hash = compute_md5(path);
                auto entry = collect_metadata(path, hash);
                storage.replace(entry);
                bar.set_progress(++current * 100.0f / total_files);
            }
            else if (std::filesystem::is_directory(path)) {
                for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
                    if (!entry.is_regular_file()) continue;
                    std::string file_path = entry.path().string();
                    if (is_excluded(file_path, exclude_paths)) continue;
                    auto hash = compute_md5(file_path);
                    auto meta_entry = collect_metadata(file_path, hash);
                    storage.replace(meta_entry);
                    bar.set_progress(++current * 100.0f / total_files);
                }
            }
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "[ERROR] 파일 접근 실패: " << e.what() << std::endl;
        }
    }

    indicators::show_console_cursor(true);
}

void BaselineGenerator::generate_and_store() {
    parse_ini_and_store();
}