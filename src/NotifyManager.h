#pragma once
#include <string>

class NotifyManager
{
public:
    enum class eUrgency
    {
        Low,
        Normal,
        Critical
    };
    static void Run(const std::string &title, const std::string &message, eUrgency level);

private:
    static std::string urgencyToString(eUrgency level);
};