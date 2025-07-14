#include "CommandReceiver.h"
#include "Paths.h"
#include "CommandExecutor.h"
#include "SocketOStream.h"
#include "ServerDaemon.h" 

#include <filesystem>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;

void CommandReceiver::Init(ServerDaemon& daemon, std::atomic<bool>& shouldRun)
{
    mpServerDaemon = &daemon;
    mpShouldRun = &shouldRun;
}

void CommandReceiver::Run()
{
    // 소켓 파일 저장 디렉토리 생성
    // 소켓 파일 : 통신 식별자, TCP의 포트 번호처럼 작동
    try 
    {
        fs::create_directories(PATH_IPC);
    } 
    catch (const fs::filesystem_error&) 
    {
        return;
    }

    // 이전 소켓 제거
    ::unlink(PATH_SOCKET);  

    // UNIX 도메인 스트림 소켓 생성
    // AF_UNIX: 로컬 파일 기반 통신
    // SOCK_STREAM: 순차적인 바이트 스트림 방식
    int mServerFd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (mServerFd < 0) 
    {
        return;
    }

    // 소켓 주소 구조체 초기화 및 타입 설정
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;

    // 소켓 경로 길이가 sun_path 버퍼 크기보다 긴지 확인
    if (std::strlen(PATH_SOCKET) >= sizeof(addr.sun_path)) 
    {
        ::close(mServerFd);
        return;
    }

    // 소켓 경로를 구조체의 sun_path에 복사
    std::strncpy(addr.sun_path, PATH_SOCKET, sizeof(addr.sun_path) - 1);
    addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';

    // 소켓에 파일 시스템 경로를 바인딩
    if (::bind(mServerFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) 
    {
        ::close(mServerFd);
        return;
    }

    // 오직 한 개의 클라이언트만 허용
    if (::listen(mServerFd, 1) < 0) 
    {
        ::close(mServerFd);
        return;
    }

    // 클라이언트 수신 대기 및 처리
    while (*mpShouldRun)
    {
        int clientFd = ::accept(mServerFd, nullptr, nullptr);
        if (clientFd < 0) continue;

        handleClient(clientFd);    // 클라이언트 요청 처리
        ::close(clientFd);
    }

    // 종료 처리: 소켓 닫고 소켓 파일 제거
    ::close(mServerFd);
    ::unlink(PATH_SOCKET);
}

void CommandReceiver::handleClient(int clientFd)
{
    char buffer[1024];
    ssize_t len = recv(clientFd, buffer, sizeof(buffer) - 1, 0);
    if (len <= 0) return;

    buffer[len] = '\0';
    std::string commandStr(buffer);
    std::vector<std::string> tokens = parseCommand(commandStr);

    SocketOStream out(clientFd);
    CommandExecutor executor;
    if (!tokens.empty() && tokens[0] == "stop") 
    {
        out.flush();
        shutdown(clientFd, SHUT_WR);
        mpServerDaemon->Stop();   // 데몬 전체 종료 요청
        return;
    }
    else 
    {
        executor.Execute(tokens, out);
    }
    out.flush();
    shutdown(clientFd, SHUT_WR);
}

std::vector<std::string> CommandReceiver::parseCommand(const std::string& commandStr)
{
    std::istringstream iss(commandStr);
    std::vector<std::string> tokens;
    std::string token;
    while (iss >> token)
        tokens.push_back(token);
    return tokens;
}
