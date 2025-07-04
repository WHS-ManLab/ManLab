#include <iostream>
#include <string>
#include "CommandHandler.h"

void printUsage() 
{
    std::cout << "Usage:\n"
              << "  ManLab man                    # Show manual\n"
              << "  ManLab malscan                # Run malware manual scan\n"
              << "  ManLab integscan              # Run integrity scan\n\n";
}

int main(int argc, char* argv[])
{
    if (argc < 2)  // 명령어 인자가 2보다 작은 경우
    {
        std::cerr << "\033[1;33m[!] No command provided.\033[0m Please use one of the following:\n\n";
        printUsage(); 
        return 1;
    }

    try
    {
        CommandHandler handler(argc, argv);
        handler.Run(); 
    }
    catch (const std::exception& e)
    {
        // 커맨드라인 파싱 중 오류 발생 시 해당 catch 문으로 전송
        // e.what에서는 사용자가 입력한 명령어 보여주기
        std::cerr << "\033[1;31m[!] Command input error:\033[0m " << e.what() << "\n\n";
        printUsage(); 
        return 1;
    }

    return 0;
}
