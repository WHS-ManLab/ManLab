#include "NotifyManager.h"
#include <cstdlib>

std::string NotifyManager::urgencyToString(eUrgency level)
{
    switch (level)
    {
    case NotifyManager::eUrgency::Low:
        return "low";
    case NotifyManager::eUrgency::Normal:
        return "normal";
    case NotifyManager::eUrgency::Critical:
        return "critical";
    default:
        return "critical";
    }
}

void NotifyManager::Run(const std::string &title, const std::string &message, eUrgency level)
{
    std::string command = "notify-send";
    command += " --app-name=ManLab";
    command += " --urgency=" + urgencyToString(level);
    command += " \'" + title + "\' \'" + message + "\'";

    int ret = system(command.c_str());

    if (ret == -1)
    {
        // 에러 로깅(fork 실패)
    }
    else
    {
        if (WIFEXITED(ret))
        {
            //  로깅 (종료 상태 코드 : WEXITSTATUS(ret))
        }
        else
        {
            // 로깅 (비정상 종료)
        }
    }
};