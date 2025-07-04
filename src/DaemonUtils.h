#pragma once

#include <string>
#include <functional>

bool IsDaemonRunning(const std::string& daemonName);
void LaunchDaemonIfNotRunning(const std::string& daemonName, std::function<void()> daemonFunc);
void StopDaemon(const std::string& daemonName);
std::string GetPidFilePath(const std::string& daemonName);