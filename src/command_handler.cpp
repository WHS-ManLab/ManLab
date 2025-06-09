#include "command_handler.h"
#include <sstream>   
#include <unistd.h>
#include <sys/wait.h> 

using namespace std;

CommandHandler::CommandHandler(int argc, char** argv) 
    : argc(argc), argv(argv), valid_command(false) {
    convert_argv();
}

void CommandHandler::convert_argv() {
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }
    
    if (args.empty()) {
        report_error_to_user();
    }
}

void CommandHandler::run() {
    classifier = CommandClassifier(args);
    config_manager = ConfigurationManager(args);
    classifier.run();  // Perform validation and classification internally

    valid_command = classifier.get_valid();
    if (!valid_command) {
        report_error_to_user(); 
        return;
    }

    command_type = classifier.get_command_type();

    if (command_type == "Config") {
        config_manager.run();
    } else if (command_type == "SigScan") {
        exec_sig_scan();
    } else if (command_type == "SigRestore") {
        exec_sig_restore();
    } else if (command_type == "FimScan") {
        exec_fim_scan();
    }
}

void CommandHandler::exec_sig_scan() {
    // TODO 
}

void CommandHandler::exec_sig_restore() {
    // TODO
}

void CommandHandler::exec_fim_scan() {
    // TODO
}

void CommandHandler::report_error_to_user() {
    std::cerr << "Invalid or missing command." << std::endl;
    exit(1);
}