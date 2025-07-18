#include <iostream>
#include <string>
#include <cstring>  
#include <sys/socket.h>    
#include <sys/un.h>        
#include <unistd.h>     
#include "CommandHandler.h"
#include "ServerDaemon.h"
#include "INIReader.h"
#include "Paths.h"

void printUsage()
{
    std::cout << "Usage:\n"
              << "  ManLab man                   # Show manual\n"
              << "  ManLab malscan               # Run malware manual scan\n"
              << "  ManLab integscan             # Run integrity scan\n\n";
}

void InitLogger() {
    // 최대 5MB, 최대 3개 파일 보관 (manlab.log, manlab.log.1, manlab.log.2)
    auto logger = spdlog::rotating_logger_mt(
        "manlab_logger", "/root/ManLab/log/manlab.log",
        1024 * 1024 * 5,  // 5MB
        3                 // 파일 개수
    );

    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::debug);
    spdlog::flush_on(spdlog::level::info);
}

int main(int argc, char* argv[])
{

    InitLogger();  // 로거 초기화

    if (argc < 2)
    {
        // 사용자를 위한 에러 메시지
        std::cerr << "\033[1;33m[!] No command provided.\033[0m Please use one of the following:\n\n";
        printUsage();
        return 1;
    }

    std::string cmd = argv[1];
    // 해당 프로세스는 데몬으로 동작하며 소켓을 열고 명령어 통신을 기다림
    // 제품 실행의 시작
    if (cmd == "run")
    {
        ServerDaemon Daemon;
        Daemon.Run(0);  
    }
    else if (cmd == "autostart")
    {
        INIReader reader(PATH_MANLAB_CONFIG_INI);

        bool shouldRun = reader.GetBoolean("Startup", "EnableManLab", true);
        if (!shouldRun)
        {
            std::cout << "\033[1;34m[INFO] ManLab is disabled by config. Skipping startup.\033[0m\n";
            return 0;
        }

        ServerDaemon daemon;
        daemon.Run(1);
    }
    else
    {
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
            std::cerr << "   \033[1;36mManLab run\033[0m 명령어로 실행하십시오.\n";
            close(sock);
            return 1;
        }

        // 명령어 파싱 (여기서 실패하면 명령어 에러)
        std::string commandToSend;
        try 
        {
            CommandHandler handler(argc, argv);
            handler.Init();
            commandToSend = handler.GetCommandString();  // 유효성 검사 포함
        }
        catch (const std::exception& e) 
        {
            std::cerr << "\033[1;31m[!] " << e.what() << "\033[0m\n";
            printUsage();
            close(sock); // 열려 있는 소켓도 닫기
            return 1;
        }

        // 서버에 명령어 전송
        send(sock, commandToSend.c_str(), commandToSend.size(), 0);

        // 서버 응답 실시간 수신 및 출력
        char buffer[256];
        while (true)
        {
            ssize_t len = recv(sock, buffer, sizeof(buffer) - 1, 0);
            if (len == 0) 
            {
                // 서버가 shutdown(fd, SHUT_WR) 또는 close(fd) 호출함 → 종료
                break;
            }
            if (len < 0) 
            {
                return 1;  // 오류 처리
            }

            ssize_t written = write(STDOUT_FILENO, buffer, len);
            if (written < 0)
            {
                perror("write");
                return 1;
            }
        }

        close(sock); // 소켓 종료
    }

    spdlog::shutdown(); // 로거 종료
    return 0;
}