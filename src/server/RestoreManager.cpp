#include "RestoreManager.h"
#include "Paths.h"
#include "DBManager.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <sys/xattr.h>
#include <spdlog/spdlog.h>

namespace fs = std::filesystem;

// static 멤버 초기화: QuarantineManager와 동일한 키와 IV를 사용
const unsigned char RestoreManager::sAesKey[33] = "0123456789abcdef0123456789abcdef";
const unsigned char RestoreManager::sAesIv[17]  = "0123456789abcdef";

// 생성자: 격리된 파일 이름을 받아 복원 준비
RestoreManager::RestoreManager(const std::string& quarantinedFileName)
    : mQuarantinedFileName(quarantinedFileName)
    , mStorage(&DBManager::GetInstance().GetQuarantineStorage())
    , mbSuccess(false)
{}

// 파일을 AES-256-CBC 방식으로 복호화
bool RestoreManager::aesDecryptFile(const std::string& inFilePath, const std::string& outFilePath)
{
    std::ifstream inFile(inFilePath, std::ios::binary);
    if (!inFile) return false;

    std::ofstream outFile(outFilePath, std::ios::binary);
    if (!outFile) return false;

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return false;

    if (1 != EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, sAesKey, sAesIv))
    {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    unsigned char inBuf[1024];
    unsigned char outBuf[1024 + AES_BLOCK_SIZE];
    int outLen = 0;

    while (inFile.read(reinterpret_cast<char*>(inBuf), sizeof(inBuf)))
    {
        int bytesRead = inFile.gcount();
        if (1 != EVP_DecryptUpdate(ctx, outBuf, &outLen, inBuf, bytesRead))
        {
            EVP_CIPHER_CTX_free(ctx);
            return false;
        }
        outFile.write(reinterpret_cast<const char*>(outBuf), outLen);
    }
    int bytesRead = inFile.gcount();
    if (bytesRead > 0)
    {
        if (1 != EVP_DecryptUpdate(ctx, outBuf, &outLen, inBuf, bytesRead))
        {
            EVP_CIPHER_CTX_free(ctx);
            return false;
        }
        outFile.write(reinterpret_cast<const char*>(outBuf), outLen);
    }

    if (1 != EVP_DecryptFinal_ex(ctx, outBuf, &outLen))
    {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    outFile.write(reinterpret_cast<const char*>(outBuf), outLen);

    EVP_CIPHER_CTX_free(ctx);
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
        spdlog::error("복구 실패: DB에서 격리 파일 메타데이터를 찾을 수 없음: {}", mQuarantinedFileName);
        return;
    }

    // 3. 경로 구성
    fs::path srcPath = fs::path(PATH_QUARANTINE) / mQuarantinedFileName;
    fs::path dstPath = meta.OriginalPath;
    fs::path tempDecryptedPath = srcPath.string() + ".dec";

    mbSuccess = false;
    try
    {
        spdlog::info("격리 파일 복호화 시작: {}", srcPath.string());
        if (aesDecryptFile(srcPath.string(), tempDecryptedPath.string())) // 임시 파일로 복호화
        {
            // 복호화된 임시 파일에 저장된 원본 권한을 적용
            fs::perms restored_perms = static_cast<fs::perms>(meta.OriginalPermissions);
            if (restored_perms != fs::perms::unknown) 
            {
                fs::permissions(tempDecryptedPath, restored_perms);
                spdlog::debug("복원 파일 권한 적용 완료: {:o}", static_cast<int>(restored_perms));
            }

            fs::rename(tempDecryptedPath, dstPath); // 원본 경로로 이동(복원)
            spdlog::info("복호화 및 복원 완료: {}", dstPath.string());

            fs::remove(srcPath); // 격리된 원본 파일 삭제
            spdlog::debug("격리 파일 삭제 완료: {}", srcPath.string());

            mbSuccess = true;
        }
        else
        {
            spdlog::error("복호화 실패: {}", srcPath.string());
            if(fs::exists(tempDecryptedPath)) 
            {
                fs::remove(tempDecryptedPath);
                spdlog::debug("임시 복호화 파일 삭제 완료: {}", tempDecryptedPath.string());
            }
        }
    }
    catch (const fs::filesystem_error& e)
    {
        spdlog::error("복구 중 파일 시스템 오류 발생: {} - {}", dstPath.string(), e.what());
        mbSuccess = false;
    }
}

bool RestoreManager::IsSuccess() const
{
    return mbSuccess;
}
