#include "CommandHandler.h"
#include "FimCommandHandler.h"
#include "SigCommandHandler.h"

#include <cstdlib>
#include <getopt.h>
#include <iostream>
#include <sstream>
#include <sys/wait.h>
#include <unistd.h>

CommandHandler::CommandHandler(int argc, char** argv)
    : mArgc(argc), mArgv(argv)
{
    try {
        parseOptions();                 // 옵션 파싱해 mArgs에 저장
    } catch (const std::exception& e) {
        std::cerr << "Initialization Error: " << e.what() << '\n';
        std::exit(EXIT_FAILURE);
    }
}

void CommandHandler::parseOptions()
{
    int optionChar;
    int optionIndex = 0;

    static struct option longOptions[] = {
        {"enable",  required_argument, nullptr, 'e'},
        {"disable", required_argument, nullptr, 'd'},
        {nullptr,   0,                nullptr,  0 }
    };

    const char* optString = "e:d:";

    while ((optionChar = getopt_long(mArgc, mArgv, optString,
                                     longOptions, &optionIndex)) != -1)
    {
        switch (optionChar) {
        case 'e':
            mArgs.emplace_back("--enable");
            mArgs.emplace_back(optarg);
            break;
        case 'd':
            mArgs.emplace_back("--disable");
            mArgs.emplace_back(optarg);
            break;
        case '?':
            throw std::invalid_argument("Unknown or malformed command-line option");
        default:
            throw std::invalid_argument("Unexpected option encountered");
        }
    }

    // 옵션 뒤의 잔여 인자 처리
    for (int i = optind; i < mArgc; ++i) {
        mArgs.emplace_back(mArgv[i]);
    }

    if (mArgs.empty()) {
        throw std::invalid_argument("No command arguments provided");
    }
}

void CommandHandler::run()
{
    if (mArgs.size() < 2) {
        reportErrorToUser();
    }

    const std::string& command = mArgs[1];
    if (command == "deamon") {
        //TODO : 데몬 실행 파트
    } else if (command == "malscan") {
        execScan();
    } else if (command == "restore") {
        if (mArgs.size() < 3) {
            std::cerr << "Missing filename for restore command.\n";
            reportErrorToUser();
        }
        execSigRestore(mArgs[2]);
    } else if (command == "integscan") {
        execFimScan();
    } else if (command == "man") {
        //TODO 사용자 매뉴얼 출력
    } else if (command == "--enable"  && mArgs.size() >= 3 &&
               mArgs[2] == "realtime_monitor")
    {
        std::cout << "Enabling realtime monitor…\n";
        // TODO: enable realtime monitor logic
    } else if (command == "--disable" && mArgs.size() >= 3 &&
               mArgs[2] == "realtime_monitor")
    {
        std::cout << "Disabling realtime monitor…\n";
        // TODO: disable realtime monitor logic
    } else {
        reportErrorToUser();
    }
}

//CLI로 출력
void CommandHandler::reportErrorToUser()
{
    std::cerr << "Invalid or missing command.\n";
    std::exit(EXIT_FAILURE);
}