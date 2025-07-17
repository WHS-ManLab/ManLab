#include <iostream>
#include "ServerDaemon.h"
#include "INIReader.h"
#include "Paths.h"

int main(int argc, char* argv[])
{
    INIReader reader(PATH_MANLAB_CONFIG_INI);

    bool shouldRun = reader.GetBoolean("Startup", "EnableManLab", true);
    if (!shouldRun)
    {
        return 0;
    }

    ServerDaemon daemon;
    daemon.Run();

    return 0;
}