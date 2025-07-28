#pragma once

#include <functional>
#include <string>
#include <vector>
#include <unordered_map>
#include <ostream>

class CommandBus 
{
public:
    using HandlerFunc = std::function<void(const std::vector<std::string>&, std::ostream&)>;

    void Register(const std::string& command, HandlerFunc handler);
    void Dispatch(const std::vector<std::string>& tokens, std::ostream& out);

private:
    std::unordered_map<std::string, HandlerFunc> mHandlers;
};