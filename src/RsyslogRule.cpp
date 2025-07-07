#include "RsyslogRule.h"

#include <iostream>
#include <regex>

//비인가 사용자의 sudo 사용 탐지
AnalysisResult AnalyzeSudoLog(const LogEntry& entry, const std::unordered_set<std::string>& rsyslogRuleSet)
{
    AnalysisResult result{ false, "", "" };

    if (entry.process.find("sudo") == std::string::npos || entry.message.find("COMMAND=") == std::string::npos)
    {
        return result;
    }

    //sudo 패스워드 실패시 탐지 X
    if (entry.message.find("incorrect password attempts") != std::string::npos)
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
            result.isMalicious = true;
            result.type = "unauthorized_sudo";
            result.description = "비인가 사용자 " + user + "가 sudo 명령 사용 : " + command;
        }
    }
    return result;
}

//다른 사용자의 passwd 변경 탐지
AnalysisResult AnalyzePasswdChangeLog(const LogEntry& entry, const std::deque<LogEntry>& recentLogs, const std::unordered_set<std::string>& rsyslogRuleSet)
{
    AnalysisResult result{ false, "", "" };
    std::regex regex(R"(pam_unix\(passwd:chauthtok\): password changed for (\w+))");
    std::smatch match;

    if (entry.process.find("passwd") != std::string::npos &&
        entry.message.find("password changed for") != std::string::npos &&
        std::regex_search(entry.message, match, regex))
    {
        std::string targetUser = match[1];
        std::string changer = "unknown";

        // recentLogs를 역순 탐색하여 sudo 로그에서 passwd 실행자를 찾는다
        for (auto it = recentLogs.rbegin(); it != recentLogs.rend(); ++it) {
            const LogEntry& log = *it;
            if (log.process.find("sudo") != std::string::npos &&
                log.message.find("COMMAND=") != std::string::npos &&
                log.message.find("passwd " + targetUser) != std::string::npos)
            {
                std::smatch sudoMatch;
                std::regex sudoRegex(R"((\w+)\s+:\s.*COMMAND=(.+))");
                if (std::regex_search(log.message, sudoMatch, sudoRegex)) {
                    changer = sudoMatch[1];
                    break;
                }
            }
        }

        if (rsyslogRuleSet.find(changer) == rsyslogRuleSet.end())
        {
            result.isMalicious = true;
            result.type = "unauthorized_passwd_change";
            result.description = changer + " (추정 사용자)가 '" + targetUser + "'의 비밀번호를 변경함";
        }
    }

    return result;
}

//passwd 반복 실패 탐지
AnalysisResult AnalyzePasswordFailureLog(const LogEntry& entry) {
    static std::unordered_map<std::string, std::pair<int, time_t>> failureMap;
    const int THRESHOLD = 3;
    const int TIME_WINDOW = 60;
    AnalysisResult result{ false, "", "" };

    // 1. sudo: N incorrect password attempts
    std::regex sudoPattern(R"((\w+)\s+:\s+(\d+)\s+incorrect password attempts)");

    // 2. 콘솔 로그인 실패
    std::regex consolPattern(R"(FAILED LOGIN \(\d+\) on '.+' FOR '(\w+)', Authentication failure)");

    // 3. GUI 첫번째 실패 이후 탐지 "message repeated N times: [ pam_unix(...) ]""
    std::regex guiPattern(R"(message repeated (\d+) times: \[ pam_unix\([^\)]+:(?:auth|account)\): authentication failure;.*user=([a-zA-Z0-9_\-]+)\])");
 
    // 4. pam_unix 인증 실패 (su/gdm/ssh 등)
    std::regex pamPattern(R"(pam_unix\((?!login:)[^\)]+:(?:auth|account)\): authentication failure;.*user=([a-zA-Z0-9_\-]+))");


    std::smatch match;
    bool matched = false;
    std::string username;
    int failCount = 0;

    // 1. sudo 실패 로그 (가장 우선 감지)
    if (entry.process == "sudo" && std::regex_search(entry.message, match, sudoPattern)) {
        username = match[1];
        failCount = std::stoi(match[2]);
        matched = true;
    }

    // 2. GUI 1번 실패 이후 N
    else if (std::regex_search(entry.message, match, guiPattern)) {
        failCount = std::stoi(match[1]);
        username = match[2];
        matched = true;
    }

    // 3. 콘솔 로그인
    else if (std::regex_search(entry.message, match, consolPattern)) {
        username = match[1];
        failCount = 1;
        matched = true;
    }

    // 4. pam_unix
    else if (std::regex_search(entry.message, match, pamPattern) && entry.process != "login") {
        username = match[1];
        failCount = 1;
        matched = true;
    }

    // 실패한 사용자 감지 안 되면 종료
    if (!matched || username.empty())
        return result;

    // 실패 누적 처리
    time_t now = RsyslogManager::ParseTime(entry.timestamp);
    auto& [count, firstTime] = failureMap[username];

    if (count == 0 || now - firstTime > TIME_WINDOW) {
        count = failCount;
        firstTime = now;
    } else {
        count += failCount;
    }

    if (count >= THRESHOLD) {
        count = 0;  // 리셋
        result.isMalicious = true;
        result.type = "brute_force_password_failure";
        result.description = "1분 내 " + std::to_string(THRESHOLD) + "회 이상 인증 실패: 사용자 '" + username + "'";
    }

    return result;
}