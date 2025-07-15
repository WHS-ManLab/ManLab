#include "QuarantineManager.h"
#include "Paths.h"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <filesystem>
#include <openssl/evp.h>
#include <openssl/aes.h>

namespace fs = std::filesystem;

// static 멤버 초기화: AES-256 키 (32바이트) 및 IV (16바이트)
// 문자열이 32바이트와 16바이트여서 배열의 마지막 부분(널값)을 고려하여, 각각 33바이트, 17바이트로 지정했습니다.
const unsigned char QuarantineManager::sAesKey[33] = "0123456789abcdef0123456789abcdef";
const unsigned char QuarantineManager::sAesIv[17]  = "0123456789abcdef";

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

// AES-256-CBC 암호화
bool QuarantineManager::aesEncryptFile(const std::string& inFilePath, const std::string& outFilePath)
{
    // 암호화할 원본 파일을 binary 모드로 연다.
    std::ifstream inFile(inFilePath, std::ios::binary);
    if (!inFile) return false;

    // 암호화된 데이터를 저장할 출력 파일을 binary 모드로 연다.
    std::ofstream outFile(outFilePath, std::ios::binary);
    if (!outFile) return false;

    // 암호화 작업을 관리할 객체 생성(알고리즘, 키, IV를 저장)
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return false;

    if (1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, sAesKey, sAesIv))
    {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    unsigned char inBuf[1024];  // 원본 파일의 데이터를 읽을 입력 버퍼
    unsigned char outBuf[1024 + AES_BLOCK_SIZE];    // 암호화된 데이터를 저장할 출력 버퍼
    int outLen = 0; // 암호화된 파일의 크기 초기화

    // 입력 파일에서 버퍼(inBuf) 크기만큼 데이터를 반복적으로 읽는다.
    while (inFile.read(reinterpret_cast<char*>(inBuf), sizeof(inBuf)))
    {
        int bytesRead = inFile.gcount();
        // EVP_EncryptUpdate 함수를 호출하여, 읽어들인 버퍼의 데이터를 암호화하고, 결과를 출력 버퍼(outBuf)에 저장
        if (1 != EVP_EncryptUpdate(ctx, outBuf, &outLen, inBuf, bytesRead))
        {
            EVP_CIPHER_CTX_free(ctx);
            return false;
        }
        outFile.write(reinterpret_cast<const char*>(outBuf), outLen);
    }
    int bytesRead = inFile.gcount();
    // 버퍼 크기 1024보다 작게 남은 데이터도 EVP_EncryptUpdate 함수를 호출해서 암호화
    if (bytesRead > 0)
    {
         if (1 != EVP_EncryptUpdate(ctx, outBuf, &outLen, inBuf, bytesRead))
        {
            EVP_CIPHER_CTX_free(ctx);
            return false;
        }
        outFile.write(reinterpret_cast<const char*>(outBuf), outLen);
    }

    if (1 != EVP_EncryptFinal_ex(ctx, outBuf, &outLen))
    {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    // 최종 암호화된 데이터를 출력 파일에 저장
    outFile.write(reinterpret_cast<const char*>(outBuf), outLen);

    // 암호화 작업 완료 후, 객체(리소스) 해제
    EVP_CIPHER_CTX_free(ctx);
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
        fs::path originalPath = info.path;
        std::string qName = originalPath.filename().string() + "_" + getCurrentDateTime();
        fs::path quarantinedPath = fs::path(PATH_QUARANTINE) / qName;
        fs::path tempEncryptedPath = quarantinedPath.string() + ".enc";

        QuarantineMetadata meta{
            info.path,
            qName,
            info.size,
            getCurrentDateTime(),
            info.cause,
            info.name,
        };

        bool success = false;
        try
        {
            fs::rename(originalPath, quarantinedPath); // 격리 폴더로 파일 이동

            // 원본 파일을 읽어서 암호화 후 암호화된 임시 파일 생성
            if (aesEncryptFile(quarantinedPath.string(), tempEncryptedPath.string()))
            {
                fs::remove(quarantinedPath); // 원본 격리 파일 삭제
                fs::rename(tempEncryptedPath, quarantinedPath); // 암호화된 파일을 최종 이름으로 변경
                success = true;
            }
            else {
                 fs::rename(quarantinedPath, originalPath); // 암호화 실패 시 원위치
            }
        }
        catch (const fs::filesystem_error& e)
        {
            //TODO : 파일시스템 오류
        }

        if(success)
        {
            mIsQuarantineSuccess[i] = true;
            mStorage->insert(meta);
        }
        else
        {
             mIsQuarantineSuccess[i] = false;
        }
    }
}

// 성공 여부 반환
const std::vector<bool>& QuarantineManager::GetIsQuarantineSuccess() const
{
    return mIsQuarantineSuccess;
}
