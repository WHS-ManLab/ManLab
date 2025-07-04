#include "RestoreManager.h"

#include <fstream>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

unsigned char RestoreManager::sEncryptionKey = 0xA5;

// 생성자: 격리된 파일 이름을 받아 복원 준비
RestoreManager::RestoreManager(const std::string& quarantinedFileName)
  : mQuarantinedFileName(quarantinedFileName)
  , mStorage(&DBManager::GetInstance().GetQuarantineStorage())
  , mbSuccess(false)
{}

// 파일 내용을 열고 바이트 단위로 XOR 복호화 수행
bool RestoreManager::applySimpleXORDecryption(const std::string& filePath)
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

void RestoreManager::Run()
{
    // 1. DB에서 격리된 파일 메타데이터 전체 조회
    auto result = mStorage->get_all<QuarantineMetadata>();
    bool found = false;
    QuarantineMetadata meta;

    // 2. 대상 파일 이름이 일치하는 항목 찾기
    for (const auto& record : result)
    {
        if (record.QuarantinedFileName == mQuarantinedFileName)
        {
            meta = record;
            found = true;
            break;
        }
    }

    if (!found)
    {
        // TODO : 에러 리포트
        return;
    }

    // 3. 경로 구성
    fs::path src = fs::path("/ManLab/quarantine") / mQuarantinedFileName;
    fs::path dst = meta.OriginalPath;

    try
    {
        // 4. 파일 이동 (rename) 후 복호화
        fs::rename(src, dst);
        mbSuccess = applySimpleXORDecryption(dst.string());
        if (!mbSuccess)
        {
            // TODO : 에러 리포트(복호화 실패)
        }
    }
    catch (const fs::filesystem_error& e)
    {
        // TODO : 에러 리포트( 파일 이동 중 오류 발생)
        mbSuccess = false;
    }
}

bool RestoreManager::IsSuccess() const
{
    return mbSuccess;
}
