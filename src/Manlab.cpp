// 바이너리의 초기 진입점
// 현재 커맨드 핸들러 없이 main내부에서 파싱하는 형태

#include <iostream>
#include <string>
#include "command_handler.h"

// 각 기능별 헤더 포함. 커맨드핸들러를 통해 다룬다면 커맨드핸들러에 포함
// #include "DaemonRunner.h"
// #include "ManualScan.h"
// #include "Scheduler.h"

void print_usage() {
    std::cout << "Usage:\n"
              << "  ./Manlab daemon                  # Run as background daemon\n"
              << "  ./Manlab malscan                # Run malware manual scan\n"
              << "  ./Manlab restore <filename>    # Restore file\n"
              << "  ./Manlab integscan            # Run integrity scan\n"
              << "  ./ManLab --enable realtime_monitor      # Enable realtime monitoring\n"
              << "  ./ManLab --disable realtime_monitor     # Disable realtime monitoring\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(); // 사용법 안내
        return 1;
    }

    try {
        CommandHandler handler(argc - 1, &argv[1]);
        handler.run(); // 명령어 실행
    } catch (const std::exception& e) {
        std::cerr << "[!]Error: " << e.what() << std::endl;
        print_usage(); // 에러 발생 시 사용법 안내
        return 1;
    }
    return 0;
}