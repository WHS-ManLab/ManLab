#include "configuration_manager.h"
#include <fstream>
#include <iostream>

using namespace std;

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
    } 
}

// Reads the entire config file and returns its content line by line
vector<string> ConfigurationManager::get_config() const {
    if(get_flag) {
        vector<string> lines;
        ifstream file(config_path);
        string line;

        while (getline(file, line)) {
            lines.push_back(line);
        }
        return lines;
    }
}

// =========================
// ==== FIM functions ======
// =========================

void ConfigurationManager::fim_add_target_path() {
    // TODO: implement target path append logic
}

void ConfigurationManager::fim_remove_target_path() {
    // TODO: implement target path remove logic
}

void ConfigurationManager::fim_show_target_path() {
    // TODO: implement target path list logic
}

void ConfigurationManager::fim_add_exclude_path() {
    // TODO: implement exclude path append logic
}

void ConfigurationManager::fim_remove_exclude_path() {
    // TODO: implement exclude path remove logic
}

void ConfigurationManager::fim_show_exclude_path() {
    // TODO: implement exclude path list logic
}

// =========================
// ==== SIG functions ======
// =========================

void ConfigurationManager::sig_add_exclude_path() {
    // TODO: implement exclude path append logic
}

void ConfigurationManager::sig_remove_exclude_path() {
    // TODO: implement exclude path remove logic
}

void ConfigurationManager::sig_show_exclude_path() {
    // TODO: implement exclude path list logic
}

void ConfigurationManager::sig_add_schedule() {
    // TODO: implement schedule append logic
}

void ConfigurationManager::sig_remove_schedule() {
    // TODO: implement schedule remove logic
}

void ConfigurationManager::sig_show_schedule() {
    // TODO: implement schedule list logic
}

// =========================
// ==== Log functions ======
// =========================

