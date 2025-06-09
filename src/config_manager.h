#ifndef CONFIGURATION_MANAGER_H
#define CONFIGURATION_MANAGER_H

#include <string>
#include <vector>

class ConfigurationManager {
public:
    // Constructor for CLI-based commands: add, remove, show
    ConfigurationManager(const std::vector<std::string>& args);
    // Constructor for external modules requesting config contents
    ConfigurationManager(const std::string& config_path);

    void run();

    // Getter: returns the full contents of the config file as vector<string>
    std::vector<std::string> get_config() const;

private:
    std::string config_path;
    std::vector<std::string> args;
    std::vector<std::string> config_string;

    // Flag to distinguish between 'get' and 'non-get' context
    bool get_flag;

    // ===== FIM-related config operations =====
    void fim_get_target_path();
    void fim_add_target_path();
    void fim_remove_target_path();
    void fim_show_target_path();

    void fim_get_exclude_path();
    void fim_add_exclude_path();
    void fim_remove_exclude_path();
    void fim_show_exclude_path();

    // Add team-specific functions as needed

    // ===== Signature-based config operations =====
    void sig_get_exclude_path();
    void sig_add_exclude_path();
    void sig_remove_exclude_path();
    void sig_show_exclude_path();

    void sig_get_schedule();
    void sig_add_schedule();
    void sig_remove_schedule();
    void sig_show_schedule();

    // Add team-specific functions as needed

    // ===== Log-analysis config operations =====

    // Add team-specific functions as needed
};

#endif 