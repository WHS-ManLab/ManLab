#include "configuration_manager.h"
#include <fstream>
#include <iostream>
#include <algorithm>

// Constructor for CLI command mode (add/remove/show)
ConfigurationManager::ConfigurationManager(const std::vector<std::string>& args)
    : args(args), get_flag(false), config_path(NULL) {}

// Constructor for config retrieval mode (used by other modules)
ConfigurationManager::ConfigurationManager(const std::string& config_path)
    : args(NULL), get_flag(true), config_path(config_path) {}

// Executes config-related commands based on CLI args
void ConfigurationManager::run() {
    if(!get_flag) {
        // TODO : parse command and call the appropriate function.
        if (args->size() < 3) { // 최소한 "-config <module> <command>" 형태
            report_error("Not enough arguments for config command. Usage: ManLab --config <module> <command> [args]");
            return;
        }

        const std::string& module = (*args)[1]; // 예: "fim", "sig", "log"
        const std::string& command = (*args)[2]; // 예: "add", "remove", "show"

        if (module == "fim") {
            if (command == "add") {
                if (args->size() < 4) { report_error("Missing path for fim add command."); return; }
                fim_add_target_path((*args)[3]);
            } else if (command == "remove") {
                if (args->size() < 4) { report_error("Missing path for fim remove command."); return; }
                fim_remove_target_path((*args)[3]);
            } else if (command == "show") {
                fim_show_target_path();
            } else if (command == "add_exclude") { // 예시: exclude path 추가 명령
                if (args->size() < 4) { report_error("Missing path for fim add_exclude command."); return; }
                fim_add_exclude_path((*args)[3]);
            } // ... 나머지 fim 관련 명령들
            else { report_error("Unknown FIM command: " + command); }
        } else if (module == "sig") {
            if (command == "add_exclude") {
                if (args->size() < 4) { report_error("Missing path for sig add_exclude command."); return; }
                sig_add_exclude_path((*args)[3]);
            } else if (command == "show_schedule") {
                sig_show_schedule();
            } // ... 나머지 sig 관련 명령들
            else { report_error("Unknown SIG command: " + command); }
        } else if (module == "log") {
            // TODO: 로그 관련 설정 명령 처리 로직
            report_error("Log configuration commands not yet implemented.");
        } else {
            report_error("Unknown configuration module: " + module);
        }
    }
}

// Reads the entire config file and returns its content line by line
std::vector<string> ConfigurationManager::get_config() const {
    if(get_flag) {
        std::vector<std::string> lines;
    if (get_flag && config_path) { // config_path 포인터가 유효한지 확인
        std::ifstream file(*config_path); // 포인터가 가리키는 값을 역참조
        if (!file.is_open()) {
            // 파일 열기 실패 시 오류 처리
            report_error("Failed to open config file: " + *config_path);
            // 필요하다면 빈 벡터를 반환하거나 예외를 던질 수 있습니다.
            return {}; // 빈 벡터 반환
        }
        std::string line;
        while (std::getline(file, line)) {
            lines.push_back(line);
        }
    } else {
        // get_flag가 false이거나 config_path가 설정되지 않은 경우 오류
        report_error("get_config() called in invalid mode or without config_path.");
    }
    return lines;
    }
}

void ConfigurationManager::report_error(const std::string& message) const {
    std::cerr << "ConfigurationManager Error: " << message << std::endl;
    std::exit(EXIT_FAILURE);
}