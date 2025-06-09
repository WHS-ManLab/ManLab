#include "command_classifier.h"

// Constructor
CommandClassifier::CommandClassifier(const std::vector<std::string>& args)
    : args(args), command_type(""), valid_command(false)
{
    valid_command = validate(); 
}

// Run the appropriate logic based on command type
void CommandClassifier::run() {
    if (!valid_command) {
        return; // Invalid command
    }
    // TODO : Determine the command type
}

// Getter for command type
std::string CommandClassifier::get_command_type() const {
    return command_type;
}

// Getter for validity
bool CommandClassifier::get_valid() const {
    return valid_command;
}

bool CommandClassifier::validate() {
    // TODO : Command validation
}