#include "CommandHandler.h"
#include <getopt.h>
#include <stdexcept>
#include <unistd.h>  // for getopt_long
#include "INIReader.h" 


CommandHandler::CommandHandler(int argc, char** argv)
    : mArgc(argc)
    , mArgv(argv)
{}

void CommandHandler::Init() 
{
    parseOptions(); 
}

// ------------------------------------------------------------
// CommandHandler::parseOptions()
// CLI 옵션을 getopt_long()으로 파싱하여 mArgs 벡터에 저장합니다.
//
// [사용법 및 확장 규칙]
// • 새 옵션을 추가하려면 아래 3개만 수정하면 됩니다:
//   ① longOptions[] : "이름", 인자유형, nullptr, '단문자'
//   ② optString     : 단문자를 나열, 인자 필요 시 ':' 추가
//   ③ switch-case   : 각 옵션에 대한 처리 로직 추가
//
// 예시:
//   longOptions[] = { {"scan", required_argument, nullptr, 's'}, ... };
//   optString = "s:"
//   case 's': mArgs.emplace_back("--scan"); mArgs.emplace_back(optarg); break;
//
// 현재는 별도의 CLI 옵션 없이 위치 인자만 처리합니다.
// ------------------------------------------------------------
void CommandHandler::parseOptions()
{
    optind = 0;
    int optionChar;
    int optionIndex = 0;

    static struct option longOptions[] =
    {
        // 예시: {"scan", required_argument, nullptr, 's'},
        {nullptr, 0, nullptr, 0}
    };

    const char* optString = "";  // 예시: "s:"

    while ((optionChar = getopt_long(mArgc,
                                     mArgv,
                                     optString,
                                     longOptions,
                                     &optionIndex)) != -1)
    {
        switch (optionChar)
        {
        /* ---------- 옵션 처리 예시 ----------
        case 's':
            mArgs.emplace_back("--scan");
            mArgs.emplace_back(optarg);
            break;
        -------------------------------------*/
        default:
            // main.cpp의 catch문으로 전송
            throw std::invalid_argument("Unknown or malformed command-line option");
        }
    }

    // 옵션 뒤에 남은 위치 인자를 저장
    if (optind < mArgc)
    {
        for (int i = optind; i < mArgc; ++i)
        {
            mArgs.emplace_back(mArgv[i]);
        }
    }
    else
    {
        for (int i = 0; i < mArgc; ++i)
        {
            mArgs.emplace_back(mArgv[i]);
        }
    }

    if (mArgs.empty())
    {
        // main.cpp의 catch문으로 전송
        throw std::invalid_argument("No command arguments provided");
    }
}

std::string CommandHandler::GetCommandString()
{
    const std::string& command = mArgs[0];

    // 명령어 유형 결정
    // [malscan] 악성코드 수동검사 명령어
    if (command == "malscan")
    {
        return "malscan";
    }

    // [restore] 격리된 파일 복구 명령어
    else if (command == "restore")
    {
        if (mArgs.size() < 2)
        {
            // main.cpp의 catch문으로 전송 
            throw std::invalid_argument("Missing filename for restore command");
        }
        return "restore " + mArgs[1];
    }

    // [integscan] 무결성 수동검사 명령어
    else if (command == "integscan")
    {
        return "integscan";
    }

    // [baseline] 
    else if (command == "baseline")
    {
        return "baseline";
    }

    // [check_baseline]
    else if (command == "check_baseline")
    {
        return "check_baseline";
    }

    // [man] 매뉴얼 디스플레이 명령어
    else if (command == "check_integscan")
    {
        return "check_integscan";
    }
    else if (command == "man")
    {
        return "man";
    }
    else if (command == "stop")
    {
        return "stop";
    }

    // [malreport] 지금까지의 검사 내역 보여주는 명령어
    else if (command == "malreport")
    {
        if (mArgs.size() < 2)
        {
            throw std::invalid_argument("Missing subcommand for malreport (expected 'list' or 'view <id>')");
        }

        const std::string& subcmd = mArgs[1];

        if (subcmd == "list")
        {
            return "malreport list";
        }
        else if (subcmd == "view")
        {
            if (mArgs.size() < 3)
            {
                throw std::invalid_argument("Missing <id> for malreport view");
            }
            return "malreport view " + mArgs[2];
        }
        else
        {
            throw std::invalid_argument("Unknown malreport subcommand: " + subcmd);
        }
    }

    // 명령어 집합에 존재하지 않는 경우
    else
    {
        // main.cpp의 catch문으로 전송
        throw std::invalid_argument("Unknown command: " + command);
    }
}
