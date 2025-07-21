#include "GmailClient.h"
#include <stdlib.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <vector>
#include <cstring>
#include <spdlog/spdlog.h>

// 컴파일 시 이 부분을 주석 처리 후 key, iv를 붙여넣으시면 됩니다.
const unsigned char key[32] = {};
const unsigned char iv[16] = {};

GmailClient::GmailClient(const std::string &to)
{
    mCurl = curl_easy_init();
    if (!mCurl)
    {
        // TODO : 에러 처리 로직
        spdlog::error("Failed to init curl.");
        return;
    }

    mTo = to;
    std::string encryptedAppPassword = "pYJ53KhgQmK4T8HLiqWggNchno0fQ+ZAhFX8NUIH2cY=";
    mAppPassword = GmailClient::DecryptAppPassword(encryptedAppPassword);
    spdlog::info("GmailClient() completed.");
}

GmailClient::~GmailClient()
{
    if (mCurl)
    {
        curl_easy_cleanup(mCurl);
    }
}

bool GmailClient::Run(const std::string &file)
{
    if (mCurl)
    {
        struct curl_slist *recipients = nullptr;
        curl_mime *mime = nullptr;
        curl_mimepart *part = nullptr;

        curl_easy_setopt(mCurl, CURLOPT_USERNAME, mFrom.c_str());
        curl_easy_setopt(mCurl, CURLOPT_PASSWORD, mAppPassword.c_str());
        curl_easy_setopt(mCurl, CURLOPT_URL, "smtps://smtp.gmail.com:465");
        curl_easy_setopt(mCurl, CURLOPT_USE_SSL, CURLUSESSL_ALL);

        std::string fromHeader = "<" + mFrom + ">";
        curl_easy_setopt(mCurl, CURLOPT_MAIL_FROM, fromHeader.c_str());

        std::string toHeader = "<" + mTo + ">";
        recipients = curl_slist_append(recipients, toHeader.c_str());
        curl_easy_setopt(mCurl, CURLOPT_MAIL_RCPT, recipients);

        mime = curl_mime_init(mCurl);
        std::string body = "만랩 로그 분석 리포트입니다.";

        // 본문
        part = curl_mime_addpart(mime);
        curl_mime_data(part, body.c_str(), CURL_ZERO_TERMINATED);
        curl_mime_type(part, "text/plain");

        // 파일 첨부
        part = curl_mime_addpart(mime);
        curl_mime_filedata(part, file.c_str());
        curl_mime_filename(part, (file.substr(20)).c_str());
        curl_mime_type(part, "test/html");
        curl_mime_encoder(part, "base64");

        // 헤더 지정
        struct curl_slist *headers = nullptr;
        headers = curl_slist_append(headers, ("To: " + mTo).c_str());
        headers = curl_slist_append(headers, ("From: " + mFrom).c_str());
        headers = curl_slist_append(headers, "Subject: 만랩 로그 분석 리포트");
        curl_easy_setopt(mCurl, CURLOPT_HTTPHEADER, headers);

        curl_easy_setopt(mCurl, CURLOPT_MIMEPOST, mime);
        CURLcode res = curl_easy_perform(mCurl);
        if (res != CURLE_OK)
        {
            spdlog::error("curl_easy_perform() failed: ", curl_easy_strerror(res));
            curl_slist_free_all(recipients);
            curl_slist_free_all(headers);
            curl_mime_free(mime);
            return false;
        }
        else
        {
            spdlog::info("Email sent successfully.");
            curl_slist_free_all(recipients);
            curl_slist_free_all(headers);
            curl_mime_free(mime);
            return true;
        }
    }
    else
    {
        spdlog::error("Failed to init curl.");
        return false;
    }
}

std::string GmailClient::DecryptAppPassword(const std::string &appPassword)
{
    std::string encrypted = Base64Decode(appPassword);
    unsigned char decrypted[128];
    int decrypted_len = 0;

    bool success = AesDecrypt(
        reinterpret_cast<const unsigned char *>(encrypted.data()),
        encrypted.length(),
        key,
        iv,
        decrypted,
        decrypted_len);

    if (!success) {
        // TODO : 에러 핸들링
        spdlog::error("App password decryption failed.");
        return "";
    }

    return std::string(reinterpret_cast<const char*>(decrypted), decrypted_len);
}

std::string GmailClient::Base64Decode(const std::string &input)
{
    BIO* bio, * b64;
    int decodeLen = input.length();
    std::vector<char> buffer(decodeLen);

    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new_mem_buf(input.data(), input.length());
    bio = BIO_push(b64, bio);
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);

    int len = BIO_read(bio, buffer.data(), decodeLen);
    BIO_free_all(bio);

    if (len <= 0) {
        // TODO: 에러 처리
        spdlog::error("Base64 decoding failed.");
        return "";
    }

    spdlog::debug("Base64 decoding successed.");
    return std::string(buffer.data(), len);
}

bool GmailClient::AesDecrypt(const unsigned char* ciphertext, int ciphertextLen,
                const unsigned char* key, const unsigned char* iv,
                unsigned char* plaintext, int& plaintextLen)
{

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
    {
        spdlog::error("Failed to create OpenSSL Context.");
        return false;
    }

    int len = 0;

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv) != 1)
    {
        spdlog::error("Decryption initialization failed in AES-256-CBC mode.");
        return false;
    }

    if (EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertextLen) != 1)
    {
        spdlog::error("Decryption failed.");
        return false;
    }
    plaintextLen = len;

    if (EVP_DecryptFinal_ex(ctx, plaintext + len, &len) != 1)
    {
        spdlog::error("Failed to remove padding.");
        return false;
    }
    plaintextLen += len;

    EVP_CIPHER_CTX_free(ctx);
    spdlog::debug("Decryption successed.");
    return true;
}