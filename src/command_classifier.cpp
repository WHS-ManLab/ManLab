#include "command_classifier.h"

// Constructor
CommandClassifier::CommandClassifier(const std::vector<std::string>& args)
    : args(args) {}

// Run the appropriate logic based on command type
void CommandClassifier::run() {
    if (std::find(args.begin(), args.end(), "-config") != args.end()) {
        command_type = "Config";
        valid_command = true;
    } else if (std::find(args.begin(), args.end(), "--sig") != args.end()) {
        if (std::find(args.begin(), args.end(), "-disable") != args.end()) {
            command_type = "SigRestore";
            valid_command = true;
        } else {
            command_type = "SigScan";
            valid_command = true;
        }
    } else if (std::find(args.begin(), args.end(), "--fim") != args.end()) {
        command_type = "FimScan";
        valid_command = true;
    } else {
        command_type = "";
        valid_command = false;
    }
}

// Getter for command type
std::string CommandClassifier::get_command_type() const {
    return command_type;
}

// Getter for validity
bool CommandClassifier::get_valid() const {
    return valid_command;
}