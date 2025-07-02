#include "NotifyManager.h"
#include <stdlib.h>

void NotifyManager::Run(const std::string &title, const std::string &message)
{
    std::string command = "notify-send --urgency=critical";
    command += " \'" + title + "\' \'" + message + "\'";

    int ret = system(command.c_str());
    
    if(ret == -1){
        // 로깅(fork 실패)
    }
    else{
        if(WIFEXITED(ret)){
            //  로깅 (종료 상태 코드 : WEXITSTATUS(ret))
        }
        else{
            // 로깅 (비정상 종료)
        }
    }
};