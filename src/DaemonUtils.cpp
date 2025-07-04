#include "DaemonUtils.h"

#include <fstream>
#include <filesystem>
#include <unistd.h>
#include <signal.h>
#include <iostream>

namespace fs = std::filesystem;

std::string GetPidFilePath(const std::string& daemonName)
{
    return "/ManLab/pid/" + daemonName + ".pid";
}

// PID파일 확인 + 실제 프로세스 확인
// SIGKILL이나 의도치 못한 상황으로 PID파일만 남아 있을 경우 대비
bool IsDaemonRunning(const std::string& daemonName)
{
    std::string path = GetPidFilePath(daemonName);
    std::ifstream pidFile(path);
    if (!pidFile.is_open())
    {
        return false;
    }

    pid_t pid;
    pidFile >> pid;
    pidFile.close();

    return (kill(pid, 0) == 0);
}

void LaunchDaemonIfNotRunning(const std::string& daemonName, std::function<void()> daemonFunc)
{
    if (IsDaemonRunning(daemonName))
    {
        // 이미 데몬이 실행 중이라면 return 
        return;
    }

    // 데몬이 실행 중이 아니라면 fork후 데몬 실행
    // PID파일을 생성. PID파일은 서비스 부팅 시 .service에서 삭제하므로 부팅 전 기록이 남아있지 않음
    pid_t pid = fork();
    if (pid == 0)
    {
        fs::create_directories("/ManLab/pid");
        std::ofstream pidFile(GetPidFilePath(daemonName));
        pidFile << getpid(); //기존 파일의 내용을 삭제하고 새로 작성. 만약 이전 PID파일 정보만 남아있고 프로세스는 꺼졌을 경우 대비
        pidFile.close();

        daemonFunc();
        exit(0);
    }
}

// PID 파일을 삭제하고 데몬을 종료
void StopDaemon(const std::string& daemonName)
{
    std::string path = GetPidFilePath(daemonName);
    std::ifstream pidFile(path);
    if (!pidFile.is_open())
    {
        return;
    }

    pid_t pid;
    pidFile >> pid;
    pidFile.close();

    if (kill(pid, SIGTERM) == 0)
    {
        fs::remove(path);
    }
}
