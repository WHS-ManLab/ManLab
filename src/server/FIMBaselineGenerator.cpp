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

// ini 경로와 DB 경로를 멤버 변수로 저장
BaselineGenerator::BaselineGenerator(const std::string& iniPath,
                                     const std::string& dbPath)
    : mIniPath(iniPath), mDbPath(dbPath)
{
}

// 파일 경로를 받아 해당 파일의 MD5 해시값을 변환
std::string BaselineGenerator::ComputeMd5(const std::string& filePath)
{
    std::ifstream file(filePath, std::ios::binary);
    if (!file) return "";

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
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(result[i]);
    }
    return oss.str();
}

// 파일의 메타데이터 수집
BaselineEntry BaselineGenerator::CollectMetadata(const std::string& path,
                                                 const std::string& md5)
{
    struct stat statBuf;
    if (stat(path.c_str(), &statBuf) != 0)
    {
        throw std::runtime_error("stat() 실패: " + path);
    }

    // 권한 문자열 생성
    std::stringstream permStream;
    permStream << ((statBuf.st_mode & S_IRUSR) ? "r" : "-")
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
        permStream.str(),
        static_cast<int>(statBuf.st_uid),
        static_cast<int>(statBuf.st_gid),
        FormatTime(statBuf.st_ctime),
        FormatTime(statBuf.st_mtime),
        static_cast<uintmax_t>(statBuf.st_size)
    };
}

// INI 파일에서 경로를 파싱하고 해당 경로와 파일/디랙토리 해시를 DB에 저장
void BaselineGenerator::ParseIniAndStore(std::ostream& out)
{
    std::ifstream iniFile(mIniPath);
    if (!iniFile)
    {
        out << "[ERROR] INI 파일 열기 실패: " << mIniPath << "\n";
        return;
    }

    auto& storage = DBManager::GetInstance().GetBaselineStorage();

    DBManager::GetInstance().GetModifiedStorage().remove_all<ModifiedEntry>();

    std::vector<std::string> targetPaths;
    std::unordered_set<std::string> excludePaths;

    std::string line;
    bool isExclude = false;

    while (std::getline(iniFile, line))
    {
        if (line.find("[EXCLUDES") != std::string::npos)
        {
            isExclude = true;
            continue;
        }
        if (line.find("[TARGETS") != std::string::npos)
        {
            isExclude = false;
            continue;
        }
        if (line.find("Path") != std::string::npos)
        {
            auto eq = line.find('=');
            if (eq != std::string::npos)
            {
                std::string path = line.substr(eq + 1);
                path.erase(0, path.find_first_not_of(" \t"));
                path.erase(path.find_last_not_of(" \t") + 1);

                if (isExclude)
                {
                    excludePaths.insert(path);
                }
                else
                {
                    targetPaths.push_back(path);
                }
            }
        }
    }

    // 전체 파일 개수 계산
    size_t totalFiles = 0;
    for (const auto& path : targetPaths)
    {
        if (std::filesystem::is_regular_file(path))
        {
            bool excluded = false;
            for (const auto& ex : excludePaths)
            {
                if (path.rfind(ex, 0) == 0) { excluded = true; break; }
            }
            if (!excluded) ++totalFiles;
        }
        else if (std::filesystem::is_directory(path))
        {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(path))
            {
                if (!entry.is_regular_file()) continue;
                std::string filePath = entry.path().string();

                bool excluded = false;
                for (const auto& ex : excludePaths)
                {
                    if (filePath.rfind(ex, 0) == 0) { excluded = true; break; }
                }
                if (!excluded) ++totalFiles;
            }
        }
    }

    if (totalFiles == 0)
    {
        out << "[INFO] 대상 파일이 없습니다.\n";
        return;
    }

    // ProgressBar 생성
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
    for (const auto& path : targetPaths)
    {
        try
        {
            if (std::filesystem::is_regular_file(path))
            {
                bool excluded = false;
                for (const auto& ex : excludePaths)
                {
                    if (path.rfind(ex, 0) == 0) { excluded = true; break; }
                }
                if (excluded) continue;

                auto hash = ComputeMd5(path);
                storage.replace(CollectMetadata(path, hash));

                out << "\033[u"
                    << "\033[1A"
                    << "\r";

                bar.set_progress(++processed * 100.0f / totalFiles);

                out << "\033[1B"
                    << "\r"
                    << "\033[2K"
                    << path << "\n";
            }
            else if (std::filesystem::is_directory(path))
            {
                for (const auto& entry : std::filesystem::recursive_directory_iterator(path))
                {
                    if (!entry.is_regular_file()) continue;
                    std::string filePath = entry.path().string();

                    bool excluded = false;
                    for (const auto& ex : excludePaths)
                    {
                        if (filePath.rfind(ex, 0) == 0) { excluded = true; break; }
                    }
                    if (excluded) continue;

                    auto hash = ComputeMd5(filePath);
                    storage.replace(CollectMetadata(filePath, hash));

                    out << "\033[u"
                        << "\033[1A"
                        << "\033[2K"
                        << "\r";
                    bar.set_progress(++processed * 100.0f / totalFiles);
                    out << "\033[1B"
                        << "\033[2K\r"
                        << filePath << "\n";
                }
            }
        }
        catch (const std::exception& ex)
        {
            out << "[ERROR] 처리 중 예외 발생: " << ex.what() << "\n";
        }
    }

    // 파일 경로 삭제
    out << "\033[1A"
        << "\033[2K"
        << "\r";

    // 완료 후 요약 정보 출력
    out << "\n[SUCCESS] Baseline 생성 완료\n";
    out << "\n";
    out << "총 검사 파일 수: " << processed << "\n";
    out << "\n";

    out << "대상 경로:\n";
    for (const auto& tp : targetPaths)
    {
        out << "  - " << tp << "\n";
    }

    out << "\n";
    out << "제외 경로:\n";
    for (const auto& ep : excludePaths)
    {
        out << "  - " << ep << "\n";
    }
}

void BaselineGenerator::GenerateAndStore(std::ostream& out)
{
    ParseIniAndStore(out);
}
