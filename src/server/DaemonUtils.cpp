#include "DaemonUtils.h"
#include "Paths.h"

#include <fstream>
#include <filesystem>
#include <unistd.h>
#include <signal.h>
#include <iostream>
#include <fcntl.h>
#include <sys/stat.h>

namespace fs = std::filesystem;

std::string GetPidFilePath(const std::string& daemonName)
{
    return std::string(PATH_PID) + "/" + daemonName + ".pid";
}

void Daemonize()
{
    // systemd에 의해 실행된다고 가정 → fork(), setsid(), chdir("/") 불필요
    // 작업 디렉토리 변경
    if (chdir("/") < 0)
    {
        std::cerr << "[ERROR] chdir() failed\n";
        exit(1);
    }

    // 파일 권한 마스크 제거
    umask(0);

    // 표준 입력/출력/에러 차단
    int fd = open("/dev/null", O_RDWR);
    if (fd < 0)
    {
        exit(1);
    }

    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    if (fd > 2) close(fd);
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

// PID 파일 생성 및 fork
void LaunchDaemonIfNotRunning(const std::string& daemonName, std::function<void()> daemonFunc)
{
    if (IsDaemonRunning(daemonName))
    {
        // 이미 데몬이 실행 중이라면 return 
        return;
    }

    Daemonize();
    fs::create_directories(PATH_PID);
    std::ofstream pidFile(GetPidFilePath(daemonName));
    pidFile << getpid(); //기존 파일의 내용을 삭제하고 새로 작성. 만약 이전 PID파일 정보만 남아있고 프로세스는 꺼졌을 경우 대비
    pidFile.close();

    daemonFunc();
}

// PID 파일을 삭제
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
}