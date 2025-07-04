#pragma once

#include <string>
#include <functional>

bool isDaemonRunning(const std::string& daemonName);
void launchDaemonIfNotRunning(const std::string& daemonName, std::function<void()> daemonFunc);
void stopDaemon(const std::string& daemonName);
std::string getPidFilePath(const std::string& daemonName);