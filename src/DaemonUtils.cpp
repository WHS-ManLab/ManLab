#include "DaemonUtils.h"
#include <fstream>
#include <filesystem>
#include <unistd.h>
#include <signal.h>
#include <iostream>

namespace fs = std::filesystem;

std::string getPidFilePath(const std::string& daemonName) {
    return "/ManLab/pid/" + daemonName + ".pid";
}

bool isDaemonRunning(const std::string& daemonName) {
    std::string path = getPidFilePath(daemonName);
    std::ifstream pidFile(path);
    if (!pidFile.is_open()) return false;

    pid_t pid;
    pidFile >> pid;
    pidFile.close();

    return (kill(pid, 0) == 0);
}

void launchDaemonIfNotRunning(const std::string& daemonName, std::function<void()> daemonFunc) {
    if (isDaemonRunning(daemonName)) {
        std::cout << "[INFO] " << daemonName << " already running.\n";
        return;
    }

    pid_t pid = fork();
    if (pid == 0) {
        fs::create_directories("/ManLab/pid");
        std::ofstream pidFile(getPidFilePath(daemonName));
        pidFile << getpid();
        pidFile.close();

        daemonFunc();
        exit(0);
    }
}

void stopDaemon(const std::string& daemonName) {
    std::string path = getPidFilePath(daemonName);
    std::ifstream pidFile(path);
    if (!pidFile.is_open()) return;

    pid_t pid;
    pidFile >> pid;
    pidFile.close();

    if (kill(pid, SIGTERM) == 0) {
        std::cout << "[INFO] " << daemonName << " stopped.\n";
        fs::remove(path);
    }
}