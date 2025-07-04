#include "RestoreManager.h"
#include <fstream>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

unsigned char RestoreManager::mEncryptionKey = 0xA5;

RestoreManager::RestoreManager(const std::string& quarantinedFileName)
    : mQuarantinedFileName(quarantinedFileName),
      mStorage(&DBManager::GetInstance().GetQuarantineStorage()),
      mSuccess(false)
{}

bool RestoreManager::applySimpleXORDecryption(const std::string& filePath) {
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

void RestoreManager::run() {
    // DB에서 해당 메타데이터 조회
    auto result = mStorage->get_all<QuarantineMetadata>();
    bool found = false;
    QuarantineMetadata meta;

    for (const auto& record : result) {
        if (record.QuarantinedFileName == mQuarantinedFileName) {
            meta = record;
            found = true;
            break;
        }
    }

    if (!found) {
        // TODO : 에러 리포트
        return;
    }

    fs::path src = fs::path("/ManLab/quarantine") / mQuarantinedFileName;
    fs::path dst = meta.OriginalPath;

    try {
        fs::rename(src, dst);
        mSuccess = applySimpleXORDecryption(dst.string());
        if (!mSuccess) {
            // TODO : 에러 리포트
        }
    } catch (const fs::filesystem_error& e) {
        // TODO : 에러 리포트
        mSuccess = false;
    }
}

bool RestoreManager::isSuccess() const {
    return mSuccess;
}
