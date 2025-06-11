#ifndef COMMAND_CLASSIFIER_H
#define COMMAND_CLASSIFIER_H

#include <vector>
#include <string>
#include <algorithm>

class CommandClassifier {
public:
    CommandClassifier(const std::vector<std::string>& args);
    void run();

    // Getters
    std::string get_command_type() const;
    bool get_valid() const;

private:
    std::vector<std::string> args;
    std::string command_type;
    bool valid_command = flase;
};

#endif // COMMAND_CLASSIFIER_H