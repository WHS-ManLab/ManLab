#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <vector>
#include <string>
#include "command_classifier.h"
#include "config_manager.h"

using namespace std;

// CommandHandler: main class that handles and executes user commands
class CommandHandler {
public :
    CommandHandler(int argc, char** argv);
    void run();

private :
    //input argument
    int argc;   
    char** argv;    // **argv[0] == ManLab** 

    vector<string> args;  // **args[0] != ManLab** 
    string command_type;
    bool valid_command;

    CommandClassifier classifier;
    ConfigurationManager config_manager;

    // Converts argv input to vector<string> args
    void convert_argv();
    
    // Execute other CPP file
    void exec_sig_scan();
    void exec_sig_restore();
    void exec_fim_scan();   
    
    //Reports a invalid command to user via external CLI
    void report_error_to_user();
};
#endif