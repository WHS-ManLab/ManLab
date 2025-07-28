#include "DaemonUtils.h"
#include "Paths.h"

#include <fstream>
#include <filesystem>
#include <unistd.h>
#include <signal.h>
#include <iostream>
#include <fcntl.h>
#include <sys/stat.h>
#include <spdlog/spdlog.h>

namespace fs = std::filesystem;

std::string GetPidFilePath(const std::string& daemonName)
{
    return std::string(PATH_PID) + "/" + daemonName + ".pid";
}

void Daemonize()
{
    // systemd에 의해 실행된다고 가정 → fork(), setsid() 불필요
    // 작업 디렉토리 변경
    if (chdir("/") < 0)
    {
        spdlog::error("작업 디렉토리 변경 실패: {}", strerror(errno));
        spdlog::error("ManLab 종료");
        exit(1);
    }

    // 파일 권한 마스크 제거
    umask(0);

    // 표준 입력/출력/에러 차단
    int fd = open("/dev/null", O_RDWR);
    if (fd < 0)
    {
        spdlog::error("/dev/null 열기 실패: {}", strerror(errno));
        spdlog::error("ManLab 종료");
        exit(1);
    }

    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    if (fd > 2) 
    {
        close(fd);
        spdlog::debug("불필요한 파일 디스크립터({}) 닫음", fd);
    }

    spdlog::debug("표준 입출력을 /dev/null로 리다이렉트");
}

// PID파일 확인 + 실제 프로세스 확인
// SIGKILL이나 의도치 못한 상황으로 PID파일만 남아 있을 경우 대비
bool IsDaemonRunning(const std::string& daemonName)
{
    std::string path = GetPidFilePath(daemonName);
    std::ifstream pidFile(path);
    if (!pidFile.is_open())
    {
        spdlog::debug("PID 파일이 존재하지 않음");
        return false;
    }

    pid_t pid;
    pidFile >> pid;
    pidFile.close();

    if (kill(pid, 0) == 0) 
    {
        spdlog::debug("PID {}가 실행 중", pid);
        return true;
    } 
    else 
    {
        return false;
    }
}

// PID 파일 생성 및 fork
void LaunchDaemonIfNotRunning(const std::string& daemonName, std::function<void()> daemonFunc)
{
    if (IsDaemonRunning(daemonName)) 
    {
        spdlog::info("ManLan이 이미 실행 중");
        return;
    }

    spdlog::info("ManLab 실행");

    Daemonize(); 
    spdlog::info("ManLab 데몬화 완료");
    try 
    {
        fs::create_directories(PATH_PID);
        spdlog::debug("PID 디렉토리 생성 {}", PATH_PID) ;
    } 
    catch (const std::exception& e) 
    {
        spdlog::error("PID 디렉토리 생성 실패 '{}': {}", PATH_PID, e.what());
        spdlog::error("ManLab 종료");
        exit(1);
    }
    std::ofstream pidFile(GetPidFilePath(daemonName));
    pidFile << getpid(); //기존 파일의 내용을 삭제하고 새로 작성. 만약 이전 PID파일 정보만 남아있고 프로세스는 꺼졌을 경우 대비
    pidFile.close();
    spdlog::info("PID 파일 생성 완료: {} → PID {}", GetPidFilePath(daemonName), getpid());
    daemonFunc();
}

// PID 파일을 삭제
void StopDaemon(const std::string& daemonName)
{
    std::string path = GetPidFilePath(daemonName);
    std::ifstream pidFile(path);
    if (!pidFile.is_open())
    {
        spdlog::warn("PID 파일이 존재하지 않음: {}", path);
        return;
    }
    pid_t pid;
    pidFile >> pid;
    pidFile.close();

    if (fs::remove(path))
    {
        spdlog::info("PID 파일 삭제 완료: {}", path);
    }
    else
    {
        spdlog::warn("PID 파일 삭제 실패 또는 존재하지 않음: {}", path);
    }
}
