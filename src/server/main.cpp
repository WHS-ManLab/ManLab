#include <iostream>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h> 

#include "ServerDaemon.h"
#include "INIReader.h"
#include "Paths.h"

void InitLogger() 
{
    // 최대 5MB, 최대 3개 파일 보관 (manlab.log, manlab.log.1, manlab.log.2)
    auto logger = spdlog::rotating_logger_mt(
        "manlab_logger", "/root/ManLab/log/manlab.log",
        1024 * 1024 * 5,  // 5MB
        3                 // 파일 개수
    );

    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::debug);
    spdlog::flush_on(spdlog::level::debug);
}

int main(int argc, char* argv[])
{
    InitLogger();
    INIReader reader(PATH_MANLAB_CONFIG_INI);
    bool shouldRun = reader.GetBoolean("Startup", "EnableManLab", true);

    if (!shouldRun) 
    {
        spdlog::info("=== ManLab disable ===");
        return 0;
    }

    
    spdlog::info("=== ManLab Daemon Starting ===");
    ServerDaemon daemon;
    daemon.Run();
    spdlog::info("=== ManLab Daemon Exited ===");

    spdlog::shutdown();

    return 0;
}

