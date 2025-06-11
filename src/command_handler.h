#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <vector>
#include <string>
#include <memory>

#include "command_classifier.h"
#include "config_manager.h"

// CommandHandler: main class that handles and executes user commands
class CommandHandler {
public :
    CommandHandler(int argc, char** argv);
    void run();

private :
    //input argument
    int argc;   
    char** argv;    // **argv[0] == ManLab** 

    std::vector<std::string> args;  // **args[0] != ManLab** 
    std::string command_type;

    // 팀별 옵션
    bool fim_flag;
    bool sig_flag;
    bool log_flag;

    // 검사 옵션
    bool enable_option;
    bool start_option;
    bool stop_option;

    std::unique_ptr<CommandClassifier> classifier;
    std::unique_ptr<ConfigurationManager> config_manager;

    // 옵션 파싱 함수
    void parse_options();

    // Converts argv input to vector<string> args
    // void convert_argv();
    
    // Execute other CPP file
    void exec_sig_scan();
    void exec_sig_restore();
    void exec_fim_scan();   
    
    //Reports a invalid command to user via external CLI
    void report_error_to_user();
};
#endif