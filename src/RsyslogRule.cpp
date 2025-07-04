#include "RsyslogRule.h"

#include <iostream>
#include <regex>

AnalysisResult AnalyzeSudoLog(const LogEntry& entry, const std::unordered_set<std::string>& rsyslogRuleSet)
{
    AnalysisResult result{ false, "" };

    if (entry.process.find("sudo") == std::string::npos || entry.message.find("COMMAND=") == std::string::npos)
    {
        return result;
    }

    std::smatch match;
    std::regex sudoRegex(R"((\w+)\s+:\s.*COMMAND=(.+))");

    if (std::regex_search(entry.message, match, sudoRegex))
    {
        std::string user = match[1];
        std::string command = match[2];

        if (rsyslogRuleSet.find(user) == rsyslogRuleSet.end())
        {
            // 디버깅용 알림
            // std::cout << "\033[1;31m[ALERT] Unauthorized sudo usage!\033[0m\n";
            // std::cout << "  user    : " << user << "\n";
            // std::cout << "  command : " << command << "\n";

            result.isMalicious = true;
            result.type = "unauthorized_sudo";
            result.description = "비인가 사용자 " + user + "가 sudo 명령 사용 : " + command;
        }
        else
        {
            // 디버깅용 알림
            // std::cout << "[INFO] Authorized sudo by " << user << ": " << command << "\n";
        }
    }

    return result;
}