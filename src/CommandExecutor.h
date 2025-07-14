#pragma once
#include <vector>
#include <string>
#include <ostream>

class CommandExecutor
{
public:
    void Execute(const std::vector<std::string>& tokens, std::ostream& out);

private:
    void ShowManual(std::ostream& out);
};