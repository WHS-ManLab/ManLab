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

    
    // 명령어 실행 함수
    void exec_fim_scan();  
    void exec_sig_scan();
    void exec_sig_restore(const std::string& sig_name);
    void enable_realtime_monitor();
    void disable_realtime_monitor();

    // 옵션 파싱 함수
    void parse_options();
    
    // 에러 처리 함수
    void report_error_to_user();
};
#endif