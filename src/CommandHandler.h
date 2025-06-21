#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <vector>
#include <string>
#include <memory> 

class CommandHandler {
public :
    CommandHandler(int argc, char** argv);
    void run();

private :
    int argc;   
    char** argv;

    std::vector<std::string> args;

    // 옵션 파싱 함수
    void parse_options();
    
    // 에러 처리 함수
    void report_error_to_user();
};
#endif