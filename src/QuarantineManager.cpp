#include "QuarantineManager.h"
#include "Paths.h"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <filesystem>

namespace fs = std::filesystem;

// static 멤버 초기화
unsigned char QuarantineManager::sEncryptionKey = 0xA5;

// 생성자
QuarantineManager::QuarantineManager(const std::vector<ScanInfo>& infos)
    : mScanInfo(infos)
    , mStorage(&DBManager::GetInstance().GetQuarantineStorage())
{
    mIsQuarantineSuccess.resize(mScanInfo.size(), false);

    if (!fs::exists(PATH_QUARANTINE))
    {
        try
        {
            fs::create_directories(PATH_QUARANTINE);
        }
        catch (const fs::filesystem_error& e)
        {
            //TODO : 오류 로그 필요
        }
    }
}

// XOR 암호화
bool QuarantineManager::applySimpleXOREncryption(const std::string& filePath)
{
    std::fstream file(filePath, std::ios::in | std::ios::out | std::ios::binary);
    if (!file)
    {
        return false;
    }

    char byte;
    while (file.get(byte))
    {
        byte ^= sEncryptionKey;
        file.seekp(-1, std::ios::cur);
        file.put(byte);
    }

    file.close();
    return true;
}

// 현재 시간 반환
std::string QuarantineManager::getCurrentDateTime() const
{
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y%m%d_%H%M%S");
    return ss.str();
}

// 격리 실행
void QuarantineManager::Run()
{
    for (size_t i = 0; i < mScanInfo.size(); ++i)
    {
        const auto& info = mScanInfo[i];
        fs::path original = info.path;
        std::string nowStr = getCurrentDateTime();
        std::string qName = original.filename().string() + "_" + nowStr;
        fs::path quarantined = fs::path(PATH_QUARANTINE) / qName;
        QuarantineMetadata meta{
            info.path,
            qName,
            info.size,
            nowStr,
            info.cause,
            info.name,
        };

        bool success = false;
        try
        {
            fs::rename(original, quarantined);
            success = applySimpleXOREncryption(quarantined.string());
        }
        catch (const fs::filesystem_error& e)
        {
            //TODO : 파일시스템 오류
        }

        mIsQuarantineSuccess[i] = success;
        mStorage->insert(meta);
    }
}

// 성공 여부 반환
const std::vector<bool>& QuarantineManager::GetIsQuarantineSuccess() const
{
    return mIsQuarantineSuccess;
}
