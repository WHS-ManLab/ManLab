#include "DaemonBase.h"
#include <csignal>
#include <unistd.h>
#include <cstdlib>
#include <iostream>
#include <fcntl.h>
#include <sys/stat.h>

volatile bool DaemonBase::running = true;

void DaemonBase::handleSignals()
{
    signal(SIGTERM, [](int){ running = false; });
    signal(SIGINT,  [](int){ running = false; });
}

void DaemonBase::daemonize()
{
    if (setsid() < 0) exit(1);
    chdir("/");
    umask(0);

    freopen("/dev/null", "r", stdin);
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);
}