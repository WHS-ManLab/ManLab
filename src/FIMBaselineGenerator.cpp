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

// ini 파일 경로와 db 경로를 변수에 저장
BaselineGenerator::BaselineGenerator(const std::string& iniPath, const std::string& dbPath)
    : mIniPath(iniPath)
    , mDbPath(dbPath)
{
}

// MD5 해시 계산하여 문자열로 반환
std::string BaselineGenerator::ComputeMd5(const std::string& filePath)
{
    std::ifstream file(filePath, std::ios::binary);
    if (!file)
    {
        return "";
    }

    MD5_CTX ctx;
    MD5_Init(&ctx);

    char buffer[4096];
    while (file.read(buffer, sizeof(buffer)) || file.gcount())
    {
        MD5_Update(&ctx, buffer, file.gcount());
    }

    unsigned char result[MD5_DIGEST_LENGTH];
    MD5_Final(result, &ctx);

    std::ostringstream oss;
    for (int i = 0; i < MD5_DIGEST_LENGTH; ++i)
    {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(result[i]);
    }

    return oss.str();
}

// 파일 경로와 MD5 값을 받아 메타데이터를 수집하여 BaselineEntry 반환
BaselineEntry BaselineGenerator::CollectMetadata(const std::string& path, const std::string& md5)
{
    struct stat statBuf;
    if (stat(path.c_str(), &statBuf) != 0)
    {
        throw std::runtime_error("stat() 실패: " + path);
    }

    std::stringstream permSs;
    permSs << ((statBuf.st_mode & S_IRUSR) ? "r" : "-")
           << ((statBuf.st_mode & S_IWUSR) ? "w" : "-")
           << ((statBuf.st_mode & S_IXUSR) ? "x" : "-")
           << ((statBuf.st_mode & S_IRGRP) ? "r" : "-")
           << ((statBuf.st_mode & S_IWGRP) ? "w" : "-")
           << ((statBuf.st_mode & S_IXGRP) ? "x" : "-")
           << ((statBuf.st_mode & S_IROTH) ? "r" : "-")
           << ((statBuf.st_mode & S_IWOTH) ? "w" : "-")
           << ((statBuf.st_mode & S_IXOTH) ? "x" : "-");

    auto FormatTime = [](time_t t)
    {
        std::tm* tmPtr = std::localtime(&t);
        std::ostringstream oss;
        oss << std::put_time(tmPtr, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    };

    return BaselineEntry{
        path,
        md5,
        permSs.str(),
        static_cast<int>(statBuf.st_uid),
        static_cast<int>(statBuf.st_gid),
        FormatTime(statBuf.st_ctime),
        FormatTime(statBuf.st_mtime),
        static_cast<uintmax_t>(statBuf.st_size)
    };
}

// 파일 경로가 제외 목록에 해당하는지 확인
bool BaselineGenerator::IsExcluded(const std::string& path, const std::set<std::string>& excludes)
{
    for (const auto& exclude : excludes)
    {
        if (path == exclude || path.rfind(exclude + "/", 0) == 0)
        {
            return true;
        }
    }
    return false;
}

// ini 파일을 파싱하고 대상 파일의 해시 및 메타데이터를 db에 저장
void BaselineGenerator::ParseIniAndStore()
{
    std::ifstream iniFile(mIniPath);
    if (!iniFile)
    {
        std::cerr << "[ERROR] INI 파일 열기 실패: " << mIniPath << std::endl;
        return;
    }

    auto& storage = DBManager::GetInstance().GetBaselineStorage();
    std::set<std::string> excludePaths;
    std::vector<std::string> targetPaths;
    std::string line;
    std::string currentSection;

    while (std::getline(iniFile, line))
    {
        if (line.empty() || line[0] == ';' || line[0] == '#')
        {
            continue;
        }

        if (line.front() == '[' && line.back() == ']')
        {
            currentSection = line.substr(1, line.size() - 2);
            continue;
        }

        size_t eqPos = line.find('=');
        if (eqPos != std::string::npos)
        {
            std::string key = line.substr(0, eqPos);
            std::string value = line.substr(eqPos + 1);

            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            if (key == "Path")
            {
                if (currentSection.rfind("EXCLUDE_", 0) == 0)
                {
                    excludePaths.insert(value);
                }
                else if (currentSection.rfind("TARGETS_", 0) == 0)
                {
                    targetPaths.push_back(value);
                }
            }
        }
    }

    indicators::show_console_cursor(false);
    size_t totalFiles = 0;
    for (const auto& path : targetPaths)
    {
        if (std::filesystem::is_directory(path))
        {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(path))
            {
                if (entry.is_regular_file())
                {
                    ++totalFiles;
                }
            }
        }
        else if (std::filesystem::is_regular_file(path))
        {
            ++totalFiles;
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

    for (const auto& path : targetPaths)
    {
        try
        {
            if (std::filesystem::is_regular_file(path))
            {
                if (IsExcluded(path, excludePaths))
                {
                    continue;
                }

                auto hash = ComputeMd5(path);
                auto entry = CollectMetadata(path, hash);
                storage.replace(entry);
                bar.set_progress(++current * 100.0f / totalFiles);
            }
            else if (std::filesystem::is_directory(path))
            {
                for (const auto& entry : std::filesystem::recursive_directory_iterator(path))
                {
                    if (!entry.is_regular_file())
                    {
                        continue;
                    }

                    std::string filePath = entry.path().string();
                    if (IsExcluded(filePath, excludePaths))
                    {
                        continue;
                    }

                    auto hash = ComputeMd5(filePath);
                    auto metaEntry = CollectMetadata(filePath, hash);
                    storage.replace(metaEntry);
                    bar.set_progress(++current * 100.0f / totalFiles);
                }
            }
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            std::cerr << "[ERROR] 파일 접근 실패: " << e.what() << std::endl;
        }
    }

    bar.set_progress(100.0f);
    indicators::show_console_cursor(true);
}

void BaselineGenerator::GenerateAndStore()
{
    ParseIniAndStore();
}
