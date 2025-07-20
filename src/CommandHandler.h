#pragma once 

#include <vector>
#include <string>

class CommandHandler 
{
public:
    /// argc/argv를 받아 명령어 실행 준비
    CommandHandler(int argc, char** argv);
    void Init();
    std::string GetCommandString();

private:
    int mArgc;
    char** mArgv;
    std::vector<std::string> mArgs;

    // getopt 기반 옵션 및 인자 파싱
    void parseOptions();
};