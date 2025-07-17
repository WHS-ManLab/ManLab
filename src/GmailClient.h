#pragma once

#include <string>
#include <curl/curl.h>

class GmailClient
{
public:
    GmailClient(const std::string &to);
    ~GmailClient();
    bool Run(const std::string &file);

private:
    std::string mFrom = "whsmanlab@gmail.com";
    std::string mTo;
    std::string mAppPassword;
    CURL *mCurl;
};
