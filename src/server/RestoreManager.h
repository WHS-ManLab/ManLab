#pragma once

#include <string>
#include "DBManager.h"

class RestoreManager {
public:
    RestoreManager(const std::string& quarantinedFileName);
    void Run();
    bool IsSuccess() const;

private:
    // AES-256 암호화 키와 초기화 벡터(IV)
    static const unsigned char sAesKey[33];
    static const unsigned char sAesIv[17];

    // AES 복호화 함수
    bool aesDecryptFile(const std::string& inFilePath, const std::string& outFilePath);

    std::string mQuarantinedFileName;
    StorageQuarantine* mStorage;
    bool mbSuccess;
};