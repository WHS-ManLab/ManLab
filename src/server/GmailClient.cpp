#include "GmailClient.h"
#include <stdlib.h>
#include <iostream> // 디버깅용

GmailClient::GmailClient(const std::string &to)
{
    mCurl = curl_easy_init();
    if (!mCurl)
    {
        std::cerr << "Failed to init curl" << std::endl;
        // TODO : 에러 처리 로직
        return;
    }

    mTo = to;
    std::string appPassword("..."); // TODO : AES 암호화
    mAppPassword = appPassword;
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
    if(mCurl)
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
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
            // TODO : 로깅
            curl_slist_free_all(recipients);
            curl_slist_free_all(headers);
            curl_mime_free(mime);
            return false;
        }
        else
        {
            std::cout << "Email sent successfully.\n";
            curl_slist_free_all(recipients);
            curl_slist_free_all(headers);
            curl_mime_free(mime);
            return true;
        }
    }
    else 
    {
        return false;
    }
}