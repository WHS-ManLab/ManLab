#include "command_handler.h"
#include "fim_command_handler.h"
#include "sig_command_handler.h"
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
    } catch (const std::exception& e) {
        std::cerr << "Initialization Error: " << e.what() << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

void CommandHandler::parse_options() {
    int c;
    int option_index = 0;

    static struct option long_options[] = {
        {"enable",  required_argument,       0,  'e' },
        {"disable", required_argument,       0,  'd' },
        {0,         0,                 0,  0 }
    };

    const char* optstring = "e:d:";

    while ((c = getopt_long(argc, argv, optstring, long_options, &option_index)) != -1) {
        switch (c) {
            case 'e': args.emplace_back("--enable"); args.emplace_back(optarg); break;
            case 'd': args.emplace_back("--disable"); args.emplace_back(optarg); break;
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
    if (args[1] == "malscan") {
        exec_sig_scan();
    } else if (args[1] == "restore") {
        if (args.size() < 2) {
            std::cerr << "Missing filename for restore command." << std::endl;
            report_error_to_user();
        }
        exec_sig_restore(args[2]);
    } else if (args[1] == "integscan") {
        exec_fim_scan();
    } else if (args[1] == "--enable" && args.size() >= 2 && args[2] == "realtime_monitor") {
        std::cout << "Enabling realtime monitor..." << std::endl;
        // TODO: enable realtime monitor logic
    } else if (args[1] == "--disable" && args.size() >= 2 && args[2] == "realtime_monitor") {
        std::cout << "Disabling realtime monitor..." << std::endl;
        // TODO: disable realtime monitor logic
    } else {
        report_error_to_user();
    }
}

void CommandHandler::report_error_to_user() {
    std::cerr << "Invalid or missing command." << std::endl;
    std::exit(EXIT_FAILURE);
}