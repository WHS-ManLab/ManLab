// 바이너리의 초기 진입점
// 현재 커맨드 핸들러 없이 main내부에서 파싱하는 형태

#include <iostream>
#include <string>

// 각 기능별 헤더 포함. 커맨드핸들러를 통해 다룬다면 커맨드핸들러에 포함
// #include "DaemonRunner.h"
// #include "ManualScan.h"
// #include "Scheduler.h"

void print_usage() {
    std::cout << "Usage:\n"
              << "  ./Manlab daemon                  # Run as background daemon\n"
              << "  ./Manlab scan --path /dir [...] # Manual scan\n"
              << "  ./Manlab schedule                # Run scheduled scan\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(); // 사용법 안내
        return 1;
    }

    std::string mode = argv[1];

    if (mode == "daemon") {
        //데몬 시행, 초기화 (Makefile 시행 시 deamon을 인자로 전달하며 시작)
    } else if (mode == "scan") {
        //수동 검사
    } else if (mood == "man") {
        // 매뉴얼 출력
    } else {
        print_usage();
        return 1;
    }
}