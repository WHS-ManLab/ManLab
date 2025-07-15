#include "RestoreManager.h"
#include "Paths.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <openssl/evp.h>
#include <openssl/aes.h>

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
        // TODO : 에러 리포트
        return;
    }

    // 3. 경로 구성
    fs::path srcPath = fs::path(PATH_QUARANTINE) / mQuarantinedFileName;
    fs::path dstPath = meta.OriginalPath;
    fs::path tempDecryptedPath = srcPath.string() + ".dec";

    mbSuccess = false;
    try
    {
        if (aesDecryptFile(srcPath.string(), tempDecryptedPath.string())) // 임시 파일로 복호화
        {
            fs::rename(tempDecryptedPath, dstPath); // 원본 경로로 이동(복원)
            fs::remove(srcPath); // 격리된 원본 파일 삭제
            mbSuccess = true;
        }
        else
        {
             if(fs::exists(tempDecryptedPath)) fs::remove(tempDecryptedPath);
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
