#include <iostream>
#include <string>
#include "CommandHandler.h"

void print_usage() {
    std::cout << "Usage:\n"
              << "  ./ManLab malscan                # Run malware manual scan\n"
              << "  ./ManLab restore <filename>    # Restore file\n"
              << "  ./ManLab integscan            # Run integrity scan\n"
              << "  ./ManLab --enable realtime_monitor      # Enable realtime monitoring\n"
              << "  ./ManLab --disable realtime_monitor     # Disable realtime monitoring\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) { // 명령어 인자가 2보다 작은 경우
        print_usage(); 
        return 1;
    }

    try {
        CommandHandler handler(argc, argv);
        handler.run(); 
    } catch (const std::exception& e) {
        std::cerr << "[!]Error: " << e.what() << std::endl;
        print_usage(); 
        return 1;
    }
    return 0;
}