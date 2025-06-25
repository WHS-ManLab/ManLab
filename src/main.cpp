<<<<<<< HEAD
#include <string>
#include "baseline_generator.h"
#include "compare_with_baseline.h"

int main() {
    std::string ini_path = "test.ini";
    std::string db_path = "baseline.db";

    BaselineGenerator generator(ini_path, db_path);
    generator.generate_and_store();
    
    compare_with_baseline(db_path);
    return 0;
}

=======
#include <iostream>
#include <string>
#include "CommandHandler.h"

void print_usage() {
    std::cout << "Usage:\n"
              << "  ./Manlab malscan                # Run malware manual scan\n"
              << "  ./Manlab restore <filename>    # Restore file\n"
              << "  ./Manlab integscan            # Run integrity scan\n"
              << "  ./Manlab --enable realtime_monitor      # Enable realtime monitoring\n"
              << "  ./Manlab --disable realtime_monitor     # Disable realtime monitoring\n";
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
>>>>>>> 8c5fb367890b21c1a6d2ad1fb2677f2a8cbca03f
