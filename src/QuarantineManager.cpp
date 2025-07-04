#include "QuarantineManager.h"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <filesystem>

namespace fs = std::filesystem;

// static 멤버 초기화
unsigned char QuarantineManager::mEncryptionKey = 0xA5;

// 생성자
QuarantineManager::QuarantineManager(const std::vector<ScanInfo>& infos)
    : mScanInfo(infos),
      mStorage(&DBManager::GetInstance().GetQuarantineStorage())
{
    mIsQuarantineSuccess.resize(mScanInfo.size(), false);

    if (!fs::exists("/ManLab/quarantine")) {
        try {
            fs::create_directories("/ManLab/quarantine");
        } catch (const fs::filesystem_error& e) {
            std::cerr << "ERROR: " << e.what() << '\n';
        }
    }
}

// XOR 암호화
bool QuarantineManager::applySimpleXOREncryption(const std::string& filePath) {
    std::fstream file(filePath, std::ios::in | std::ios::out | std::ios::binary);
    if (!file) return false;
    char byte;
    while (file.get(byte)) {
        byte ^= mEncryptionKey;
        file.seekp(-1, std::ios::cur);
        file.put(byte);
    }
    file.close();
    return true;
}

// 현재 시간 반환
std::string QuarantineManager::getCurrentDateTime() const {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y%m%d_%H%M%S");
    return ss.str();
}

// 격리 실행
void QuarantineManager::run() {
    for (size_t i = 0; i < mScanInfo.size(); ++i) {
        const auto& info = mScanInfo[i];

        fs::path original = info.path;
        std::string qName = original.filename().string() + "_" + getCurrentDateTime();
        fs::path quarantined = fs::path("/ManLab/quarantine") / qName;

        QuarantineMetadata meta{
            info.path,
            qName,
            info.size,
            getCurrentDateTime(),
            info.cause,
            info.name,
        };

        bool success = false;
        try {
            fs::rename(original, quarantined);
            success = applySimpleXOREncryption(quarantined.string());
        } catch (const fs::filesystem_error& e) {
            std::cerr << "ERROR: " << e.what() << '\n';
        }

        mIsQuarantineSuccess[i] = success;
        mStorage->insert(meta);
    }
}

// 성공 여부 반환
const std::vector<bool>& QuarantineManager::GetIsQuarantineSuccess() const {
    return mIsQuarantineSuccess;
}
