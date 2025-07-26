#include <iostream>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h> 

#include "ServerDaemon.h"
#include "INIReader.h"
#include "Paths.h"

void InitLogger() 
{
    // 로그 패턴 설정 (로거 이름 제거, TID 표시)
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [tid : %t] [%^%l%$] %v");

    // 최대 5MB, 최대 3개 파일 보관 (manlab.log, manlab.log.1, manlab.log.2)
    auto logger = spdlog::rotating_logger_mt(
        "manlab_logger", PATH_MANLAB_LOG,
        1024 * 1024 * 5,  // 5MB
        3                 // 파일 개수
    );

    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::debug);
    spdlog::flush_on(spdlog::level::debug);

    //FIM 실시간 검사 로깅 선언
    auto RealTime_logger = spdlog::rotating_logger_mt(
        "RealTime_logger", PATH_MANLAB_FIM_LOG,
        1024 * 1024 * 5, // 최대 파일 크기 5MB
        3                // 최대 파일 개수 3개
    );
    
    RealTime_logger->set_level(spdlog::level::info);
    RealTime_logger->flush_on(spdlog::level::info);
}

int main(int argc, char* argv[])
{
    InitLogger();
    INIReader reader(PATH_MANLAB_CONFIG_INI);
    bool shouldRun = reader.GetBoolean("Startup", "EnableManLab", true);

    if (!shouldRun)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        
    {
        spdlog::info("EnableManLab=false in {}", PATH_MANLAB_CONFIG_INI);
        return 0;
    }

    
    spdlog::info("ManLab 시작");
    ServerDaemon daemon;
    daemon.Run();
    spdlog::info("ManLab 종료");

    spdlog::shutdown();

    return 0;
}