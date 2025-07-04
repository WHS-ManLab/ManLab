#pragma once

#include <string>
#include "DBManager.h"

class RestoreManager {
public:
    RestoreManager(const std::string& quarantinedFileName);
    void Run();
    bool IsSuccess() const;

private:
    bool applySimpleXORDecryption(const std::string& filePath);

    std::string mQuarantinedFileName;
    StorageQuarantine* mStorage;
    bool mbSuccess;
    static unsigned char sEncryptionKey;
};