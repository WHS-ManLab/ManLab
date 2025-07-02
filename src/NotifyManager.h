#pragma once
#include <string>

class NotifyManager
{
public:
    static void Run(const std::string& title, const std::string& message);
};