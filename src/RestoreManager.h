#pragma once
#include <string>
#include "DBManager.h"

class RestoreManager {
public:
    RestoreManager(const std::string& quarantinedFileName);  // explicit 제거됨
    void run();
    bool isSuccess() const;

private:
    bool applySimpleXORDecryption(const std::string& filePath);

    std::string mQuarantinedFileName;
    StorageQuarantine* mStorage;
    bool mSuccess;
    static unsigned char mEncryptionKey;
};