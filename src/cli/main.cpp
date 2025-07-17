#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "CommandHandler.h"
#include "Paths.h"

void printUsage()
{
    std::cout << "Usage:\n"
              << "  ManLab man                   # Show manual\n"
              << "  ManLab malscan               # Run malware manual scan\n"
              << "  ManLab integscan             # Run integrity scan\n\n";
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "\033[1;33m[!] No command provided.\033[0m\n\n";
        printUsage();
        return 1;
    }

    // 소켓 생성
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "\033[1;31m[!] Failed to create socket.\033[0m\n";
        return 1;
    }

    // 주소 설정
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, PATH_SOCKET, sizeof(addr.sun_path) - 1);

    // 연결 시도
    if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) 
    {
        std::cerr << "\033[1;31m[!] 제품 실행 중이 아닙니다.\033[0m\n";
        std::cerr << "   \033[1;36m서비스 파일을 수정한 뒤 재부팅해주십시오.\n";
        close(sock);
        return 1;
    }

    // 명령어 파싱
    std::string commandToSend;
    try 
    {
        CommandHandler handler(argc, argv);
        handler.Init();
        commandToSend = handler.GetCommandString();
    }
    catch (const std::exception& e) 
    {
        std::cerr << "\033[1;31m[!] " << e.what() << "\033[0m\n";
        printUsage();
        close(sock);
        return 1;
    }

    // 명령어 전송
    send(sock, commandToSend.c_str(), commandToSend.size(), 0);

    // 서버 응답 출력
    char buffer[256];
    while (true)
    {
        ssize_t len = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (len <= 0) break;
        write(STDOUT_FILENO, buffer, len);
    }

    close(sock);
    return 0;
}