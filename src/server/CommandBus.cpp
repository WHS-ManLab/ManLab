#include "CommandBus.h"

void CommandBus::Register(const std::string& command, HandlerFunc handler) 
{
    mHandlers[command] = handler;
}

void CommandBus::Dispatch(const std::vector<std::string>& tokens, std::ostream& out) {
    if (tokens.empty()) 
    {
        out << "[!] 명령이 비어 있습니다.\n";
        return;
    }

    const std::string& cmd = tokens[0];
    auto it = mHandlers.find(cmd);
    if (it != mHandlers.end()) 
    {
        it->second(tokens, out);
    } else 
    {
        out << "[!] 알 수 없는 명령입니다: " << cmd << "\n";
    }
}