#include "RsyslogRule.h"
#include "RsyslogManager.h"

#include <iostream>
#include <regex>

// 명령어 사용자 추적을 위한 함수 sudo 사용자 -> sudo 사용자, 아닐 경우 root
std::string InferChanger(const std::deque<LogEntry>& recentLogs, const std::vector<std::string>& targetCmds)
{
    for (auto it = recentLogs.rbegin(); it != recentLogs.rend(); ++it) {
        const LogEntry& log = *it;
        if (log.process == "sudo" &&
            log.message.find("COMMAND=") != std::string::npos) {

            for (const auto& cmd : targetCmds) {
                if (log.message.find(cmd) != std::string::npos) {
                    std::smatch match;
                    std::regex sudoRegex(R"((\w+)\s+:\s.*COMMAND=)");
                    if (std::regex_search(log.message, match, sudoRegex)) {
                        return match[1];
                    }
                }
            }
        }
    }
    return "root"; // fallback
}

AnalysisResult AnalyzeSudoLog(const LogEntry& entry, const std::unordered_set<std::string>& rsyslogRuleSet)
{
    AnalysisResult result{ false, "", "", "" };

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
            result.username = user;
            result.type = "T1548.003";
            result.description = "비인가 사용자 " + user + "가 sudo 명령 사용 : " + command;
        }
    }
    return result;
}

AnalysisResult AnalyzePasswdChangeLog(const LogEntry& entry, const std::deque<LogEntry>& recentLogs, const std::unordered_set<std::string>& rsyslogRuleSet)
{
    AnalysisResult result{ false, "", "", "" };
    std::regex regex(R"(pam_unix\(passwd:chauthtok\): password changed for (\w+))");
    std::smatch match;

    if (entry.process.find("passwd") != std::string::npos &&
        entry.message.find("password changed for") != std::string::npos &&
        std::regex_search(entry.message, match, regex))
    {
        std::string targetUser = match[1];
        std::string changer = InferChanger(recentLogs, { "passwd " + targetUser });

        if (rsyslogRuleSet.find(changer) == rsyslogRuleSet.end())
        {
            result.isMalicious = true;
            result.username = changer;
            result.type = "T1098";
            result.description = changer + " (추정 사용자)가 '" + targetUser + "'의 비밀번호를 변경함";
        }
    }

    return result;
}

AnalysisResult AnalyzePasswordFailureLog(const LogEntry& entry)
{
    AnalysisResult result{ false, "", "", "" };
    static std::unordered_map<std::string, std::pair<int, time_t>> failureMap;
    const int PASSWORD_FAIL_THRESHOLD = 3;
    const int PASSWORD_FAIL_TIME_WINDOW = 60;

    // 1. sudo: N incorrect password attempts
    std::regex sudoPattern(R"((\w+)\s+:\s+(\d+)\s+incorrect password attempts)");

    // 2. 콘솔 로그인 실패
    std::regex consolPattern(R"(FAILED LOGIN \(\d+\) on '.+' FOR '(\w+)', Authentication failure)");

    // 3. GUI 첫번째 실패 이후 탐지 "message repeated N times: [ pam_unix(...) ]""
    std::regex guiPattern(R"(message repeated (\d+) times: \[ pam_unix\([^\)]+:(?:auth|account)\): authentication failure;.*user=([a-zA-Z0-9_\-]+)\])");

    // 4. pam_unix 인증 실패 (su/gdm/ssh 등 콘솔은 제외)
    std::regex pamPattern(R"(pam_unix\((?!login:)[^\)]+:(?:auth|account)\): authentication failure;.*ruser=([a-zA-Z0-9_\-]+).*user=([a-zA-Z0-9_\-]+))");

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
        username = match[2];
        failCount = 1;
        matched = true;
    }

    // 실패한 사용자 감지 안 되면 종료
    if (!matched || username.empty()){
        return result;
    }

    // 실패 누적 처리
    time_t now = RsyslogManager::ParseTime(entry.timestamp);
    auto& [count, firstTime] = failureMap[username];

    if (count == 0 || now - firstTime > PASSWORD_FAIL_TIME_WINDOW) {
        count = failCount;
        firstTime = now;
    } else {
        count += failCount;
    }

    if (count >= PASSWORD_FAIL_THRESHOLD) {
        count = 0;  // 리셋
        result.isMalicious = true;
        result.type = "T1110.001";
        result.description = "1분 내 " + std::to_string(PASSWORD_FAIL_THRESHOLD) + "회 이상 인증 실패: 사용자 '" + username + "'";
        // sudo와 pam 로그인일 경우에만 실행한 유저명의 나오므로 저장.
        if (entry.process == "sudo") {
            result.username = username;
        }
        else if (std::regex_search(entry.message, match, pamPattern) && entry.process != "login"){
            result.username = match[1];
        }
    }

    return result;
}

AnalysisResult AnalyzeSudoGroupChangeLog(const LogEntry& entry, const std::deque<LogEntry>& recentLogs)
{
    AnalysisResult result{ false, "", "", "unknown" };
    std::smatch match;

    std::regex sudoAddRegex(R"(add\s+'(\w+)'\s+to\s+group\s+'sudo')");
    // sudo 권한 제거 (gpasswd)
    std::regex sudoRemovedByRegex(R"(user\s+(\w+)\s+removed\s+by\s+\w+\s+from\s+group\s+sudo)");
    // sudo 권한 변경 (deluser)
    std::regex sudoSetRegex(R"(members of group sudo set by \w+ to\s*(.*))");    

    if (std::regex_search(entry.message, match, sudoAddRegex)) {
        std::string user = match[1];
        result.isMalicious = true;
        result.username = InferChanger(recentLogs, { "usermod" });
        result.type = "T1098.007";
        result.description = "사용자 " + user + "가 sudo 그룹에 추가됨";
        return result;
    }
   
    if (std::regex_search(entry.message, match, sudoRemovedByRegex)) {
        std::string user = match[1];
        result.isMalicious = true;
        result.username = InferChanger(recentLogs, { "gpasswd" });
        result.type = "T1098.007";
        result.description = "사용자 " + user + "가 sudo 그룹에서 제거됨";
        return result;
    }
    
    if (std::regex_search(entry.message, match, sudoSetRegex)) {
        std::string members = match[1];
        result.isMalicious = true;
        result.username = InferChanger(recentLogs, { "deluser" });
        result.type = "T1098.007";

        if (members.empty()) {
            result.description = "sudo 그룹의 모든 구성원이 제거됨";
        } else {
            result.description = "sudo 그룹 구성원이 변경됨 : " + members;
        }

        return result;
    }

    return result;
}

AnalysisResult AnalyzeUserChangeLog(const LogEntry& entry, const std::deque<LogEntry>& recentLogs)
{
    AnalysisResult result{ false, "", "", "unknown" };
    std::smatch match;

    std::regex userAddRegex(R"(new user: name=(\w+))");
    std::regex userDelRegex(R"(delete user '(\w+)')");

    if (std::regex_search(entry.message, match, userAddRegex)) {
        std::string user = match[1];
        result.isMalicious = true;
        result.username = InferChanger(recentLogs, { "useradd", "adduser" });
        result.type = "T1136.001";
        result.description = "사용자 " + user + "가 생성됨";
        return result;
    }

    if (std::regex_search(entry.message, match, userDelRegex)) {
        std::string user = match[1];
        result.isMalicious = true;
        result.username = InferChanger(recentLogs, { "userdel", "deluser" });
        result.type = "T1531";
        result.description = "사용자 " + user + "가 삭제됨";
        return result;
    }

    return result;
}

AnalysisResult AnalyzeGroupChangeLog(const LogEntry& entry, const std::deque<LogEntry>& recentLogs)
{
    AnalysisResult result{ false, "", "", "unknown" };
    std::smatch match;

    std::regex groupAddRegex(R"(new group: name=(\w+))");
    std::regex groupDelRegex(R"(group '(\w+)' removed)");

    if (std::regex_search(entry.message, match, groupAddRegex)) {
        std::string group = match[1];
        result.isMalicious = true;
        result.username = InferChanger(recentLogs, { "groupadd", "addgroup" });
        result.type = "T1098";
        result.description = "그룹 " + group + "가 생성됨";
        return result;
    }

    if (std::regex_search(entry.message, match, groupDelRegex)) {
        //그룹 삭제는 로그가 3번 기록되므로 중복 기록 방지
        if (entry.message.find("removed from") != std::string::npos) {
            return result;
        }
        std::string group = match[1];
        result.isMalicious = true;
        result.username = InferChanger(recentLogs, { "groupdel", "delgroup" });
        result.type = "T1098";
        result.description = "그룹 " + group + "가 삭제됨";
        return result;
    }

    return result;
}

AnalysisResult AnalyzeGroupMemberChangeLog(const LogEntry& entry, const std::deque<LogEntry>& recentLogs)
{
    AnalysisResult result{ false, "", "", "unknown" };
    std::smatch match;

    std::regex addMemberRegex(R"(add\s+'(\w+)'\s+to\s+group\s+'(\w+)')");
    // 그룹 멤버 제거 (gpasswd)
    std::regex groupRemovedByRegex(R"(user\s+(\w+)\s+removed\s+by\s+\w+\s+from\s+group\s+(\w+))");
    // 그룹 멤버 제거 (deluser)
    std::regex groupSetRegex(R"(members of group (\w+)\s+set by \w+\s+to\s*(.*))");

    if (std::regex_search(entry.message, match, addMemberRegex)) {
        std::string user = match[1];
        std::string group = match[2];

        if (group == "sudo")
            return result;

        result.isMalicious = true;
        result.username = InferChanger(recentLogs, { "usermod" });
        result.type = "T1098.007";
        result.description = "사용자 " + user + "가 그룹 '" + group + "'에 추가됨";
        return result;
    }

    if (std::regex_search(entry.message, match, groupRemovedByRegex)) {
        std::string user = match[1];
        std::string group = match[2];

        if (group == "sudo")
            return result;

        result.isMalicious = true;
        result.username = InferChanger(recentLogs, { "gpasswd" });
        result.type = "T1098.007";
        result.description = "사용자 " + user + "가 그룹 '" + group + "'에서 제거됨";

        return result;
    }


    if (std::regex_search(entry.message, match, groupSetRegex)) {
        std::string group = match[1];
        std::string members = match[2];

        if (group == "sudo")
            return result;

        result.isMalicious = true;
        result.username = InferChanger(recentLogs, { "deluser" });
        result.type = "T1098.007";

        if (members.empty()) {
            result.description = "그룹 '" + group + "'의 모든 구성원이 제거됨";
        } else {
            result.description = "그룹 '" + group + "' 구성원이 변경됨 : " + members;
        }

        return result;
    }

    return result;
}