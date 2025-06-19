#include "command_handler.h"
#include "command_classifier.h"
#include "config_manager.h"
#include <iostream>
#include <sstream>   
#include <unistd.h>
#include <getopt.h>
#include <sys/wait.h> 
#include <cstdlib>

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
        {"config",  required_argument, 0,  'c' },
        {"enable",  no_argument,       0,  'e' },
        {"disable", no_argument,       0,  'd' },
        {"start",   required_argument, 0,  's' },
        {"stop",    no_argument,       0,  'p' },
        {0,         0,                 0,  0 }
    };

    const char* optstring = "FSLc:eds:p";

    while ((c = getopt_long(argc, argv, optstring, long_options, &option_index)) != -1) {
        switch (c) {
            case 'F': args.emplace_back("--fim"); break;
            case 'S': args.emplace_back("--sig"); break;
            case 'L': args.emplace_back("--log"); break;
            case 'c': args.emplace_back("-config"); args.emplace_back(optarg); break;
            case 'e': args.emplace_back("-enable"); break;
            case 'd': args.emplace_back("-disable"); break;
            case 's': args.emplace_back("-start"); args.emplace_back(optarg); break;
            case 'p': args.emplace_back("-stop"); break;
            case '?': throw std::invalid_argument("Unknown or malformed command-line option");
            default: throw std::invalid_argument("Unexpected option encountered");
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
        std::exit(EXIT_FAILURE);
    }
}

void CommandHandler::report_error_to_user() {
    std::cerr << "Invalid or missing command." << std::endl;
    std::exit(EXIT_FAILURE);
}