#pragma once
#include "CommandBus.h"
#include <atomic>
#include <string>
#include <vector>

class ServerDaemon;

class CommandReceiver
{
public:
    CommandReceiver() = default;

    void Init(ServerDaemon& daemon, std::atomic<bool>& shouldRun);
    void Run();  // 명령 수신 루프

private:
    void handleClient(int clientFd);
    std::vector<std::string> parseCommand(const std::string& commandStr);

    ServerDaemon* mpServerDaemon = nullptr;
    std::atomic<bool>* mpShouldRun = nullptr;
    CommandBus mCommandBus;
};
