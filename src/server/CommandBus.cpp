#include "CommandBus.h"
#include <spdlog/spdlog.h>

void CommandBus::Register(const std::string& command, HandlerFunc handler) 
{
    mHandlers[command] = handler;
}

void CommandBus::Dispatch(const std::vector<std::string>& tokens, std::ostream& out) {
    if (tokens.empty()) 
    {
        out << "[!] 명령이 비어 있습니다.\n";
        spdlog::warn("빈 명령어 수신됨");
        return; //ManLab 종료 아님
    }

    const std::string& cmd = tokens[0];
    auto it = mHandlers.find(cmd);
    if (it != mHandlers.end()) 
    {
        spdlog::info("명령어 실행 요청: {}", cmd);
        it->second(tokens, out);
    } 
    else 
    {
        out << "[!] 알 수 없는 명령입니다: " << cmd << "\n";
        spdlog::warn("알 수 없는 명령 수신됨: {}", cmd);
    }
}