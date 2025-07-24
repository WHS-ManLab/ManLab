#pragma once

#include <string>
#include <curl/curl.h>

class GmailClient
{
public:
    GmailClient(const std::string &to);
    ~GmailClient();
    bool Run(const std::string &file);
    static std::string DecryptAppPassword(const std::string &appPassword);
    static std::string Base64Decode(const std::string &input);
    static bool AesDecrypt(const unsigned char* ciphertext, int ciphertextLen,
                    const unsigned char* key, const unsigned char* iv,
                    unsigned char* plaintext, int& plaintextLen);

private:
    std::string mFrom = "whsmanlab@gmail.com";
    std::string mTo;
    std::string mAppPassword;
    CURL *mCurl;
};
