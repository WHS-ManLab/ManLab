#include "command_handler.h"
#include <iostream>
#include <sstream>   
#include <unistd.h>
#include <getopt.h>
#include <sys/wait.h> 

CommandHandler::CommandHandler(int argc, char** argv) 
    : argc(argc), argv(argv) {
    try {
        parse_options(); // 옵션을 파싱하고 멤버변수에 저장
        // 파싱된 args를 기반으로 객체 생성
        classifier = std::make_unique<CommandClassifier>(args);
        config_manager = std::make_unique<ConfigurationManager>(args);
    } catch (const std::exception& e) {
        std::cerr << "Initialization Error: " << e.what() << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

void CommandHandler::parse_options() {
    int c;
    int option_index = 0;

    static struct option long_options[] = {
        {"fim",     no_argument,       0,  'F' },
        {"sig",     no_argument,       0,  'S' },
        {"log",     no_argument,       0,  'L' },
        {"config",  no_argument,       0,  'c' },
        {"enable",  no_argument,       0,  'e' },
        {"disable", no_argument,       0,  'd' },
        {"start",   no_argument,       0,  's' },
        {"stop",    no_argument,       0,  'p' },
        {0,         0,                 0,  0 }
    };

    const char* optstring = "FSLcedsph?";

    while ((c = getopt_long(argc, argv, optstring, long_options, &option_index)) != -1) {
        switch (c) {
            case 'F': // --fim 옵션
                fim_flag = true;
                break;
            case 'S': // --sig 옵션
                sig_flag = true;
                break;
            case 'L': // --log 옵션
                log_flag = true;
                break;
            case 'c' : // -config 옵션
            case 'e': // -e 또는 --enable 옵션
                enable_option = true;
                break;
            case 'd': // -d 또는 --disable 옵션
                enable_option = false;
                break;
            case 's': // -s 또는 --start 옵션
                start_option = true;
                break;
            case 'p': // -p 또는 --stop 옵션
                stop_option = true;
                break;
            case '?': // 알 수 없는 옵션 또는 필수 인자 누락
                throw std::invalid_argument("Unknown or malformed command-line option");
            default: // 예상치 못한 경우
                throw std::invalid_argument("Unknown option character encountered");
        }
    }

    // 옵션 파싱 후 뒤의 인자들을 args 벡터에 저장
    for (int i = optind; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }

    if (args.empty()) {
        throw std::invalid_argument("No command arguments provided");
    }
}

void CommandHandler::run() {
    classifier->run();

    if (!classifier->get_valid()) {
        report_error_to_user();
        return;
    }

    command_type = classifier->get_command_type();

    if (command_type == "Config") {
        config_manager->run();
    } else if (command_type == "SigScan") {
        exec_sig_scan();
    } else if (command_type == "SigRestore") {
        exec_sig_restore();
    } else if (command_type == "FimScan") {
        exec_fim_scan();
    } else {
        std::cerr << "Unknown command type: " << command_type << std::endl;
    }
}

void CommandHandler::report_error_to_user() {
    cerr << "Invalid or missing command." << endl;
    exit(1);
}