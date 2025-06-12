#include "config_manager.h"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <filesystem>
#include <algorithm>

const std::string TARGET_FILE = "fim_targets.ini";
const std::string EXCLUDES_FILE = "fim_exlcudes.ini";

// ini 파일 벡터로 읽어오는 함수
std::vector<std::string> read_ini_sections(const std::string& filename) {
    std::ifstream file(filename);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    return lines;
}

// ini파일에 내용 작성하는 함수
void write_ini_sections(const std::string& filename, const std::vector<std::string>& lines) {
    std::ofstream file(filename, std::ios::trunc);
    for (const auto& line : lines) {
        file << line << "\n";
    }
}

// 감시 파일 추가 함수
void ConfigurationManager::fim_add_target_path() {
    std::string path, events, type, realtime, recursive;
    std::cout << "Enter target path: "; std::getline(std::cin, path);
    std::cout << "Events (e.g., open): "; std::getline(std::cin, events);
    std::cout << "Type (file/directory): "; std::getline(std::cin, type);
    std::cout << "RealtimeScan (enable/disable): "; std::getline(std::cin, realtime);
    std::cout << "Recursive (yes/no): "; std::getline(std::cin, recursive);

    std::vector<std::string> lines = read_ini_sections(TARGETS_FILE);
    int count = 1;
    for (const auto& line : lines) {
        if (line.find("[TARGETS_") == 0) count++;
    }
    std::ostringstream oss;
    oss << "[TARGETS_" << count << "]\n";
    oss << "Path = " << path << "\n";
    oss << "Events = " << events << "\n";
    oss << "Type = " << type << "\n";
    oss << "RealtimeScan = " << realtime << "\n";
    oss << "Recursive = " << recursive;

    lines.push_back("");
    std::istringstream new_entry(oss.str());
    std::string line;
    while (std::getline(new_entry, line)) {
        lines.push_back(line);
    }
    write_ini_sections(TARGETS_FILE, lines);
    std::cout << "Target path added." << std::endl;
}

// 감시 파일 제거 함수
void ConfigurationManager::fim_remove_target_path() {
    std::string target_path;
    std::cout << "Enter target path to remove: ";
    std::getline(std::cin, target_path);

    std::vector<std::string> lines = read_ini_sections(TARGETS_FILE);
    std::vector<std::string> new_lines;
    bool skip = false;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (lines[i].find("[TARGETS_") == 0) {
            skip = false;
        } else if (lines[i].find("Path = ") == 0 && lines[i].substr(7) == target_path) {
            skip = true;
            continue;
        }
        if (!skip) new_lines.push_back(lines[i]);
    }
    write_ini_sections(TARGETS_FILE, new_lines);
    std::cout << "Target path removed if matched." << std::endl;
}

// 감시 파일 목록 출력 함수
void ConfigurationManager::fim_show_target_path() {
    std::vector<std::string> lines = read_ini_sections(TARGETS_FILE);
    for (const auto& line : lines) {
        std::cout << line << std::endl;
    }
}

// 예외 파일 추가 함수
void ConfigurationManager::fim_add_exclude_path() {
    std::string path, type, recursive;
    std::cout << "Enter exclude path: "; std::getline(std::cin, path);
    std::cout << "Type (file/directory): "; std::getline(std::cin, type);
    std::cout << "Recursive (yes/no): "; std::getline(std::cin, recursive);

    std::vector<std::string> lines = read_ini_sections(EXCLUDES_FILE);
    int count = 1;
    for (const auto& line : lines) {
        if (line.find("[EXCLUDES_") == 0) count++;
    }
    std::ostringstream oss;
    oss << "[EXCLUDES_" << count << "]\n";
    oss << "Path = " << path << "\n";
    oss << "Type = " << type << "\n";
    oss << "Recursive = " << recursive;

    lines.push_back("");
    std::istringstream new_entry(oss.str());
    std::string line;
    while (std::getline(new_entry, line)) {
        lines.push_back(line);
    }
    write_ini_sections(EXCLUDES_FILE, lines);
    std::cout << "Exclude path added." << std::endl;
}

// 예외 파일 제거 함수
void ConfigurationManager::fim_remove_exclude_path() {
    std::string target_path;
    std::cout << "Enter exclude path to remove: ";
    std::getline(std::cin, target_path);

    std::vector<std::string> lines = read_ini_sections(EXCLUDES_FILE);
    std::vector<std::string> new_lines;
    bool skip = false;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (lines[i].find("[EXCLUDES_") == 0) {
            skip = false;
        } else if (lines[i].find("Path = ") == 0 && lines[i].substr(7) == target_path) {
            skip = true;
            continue;
        }
        if (!skip) new_lines.push_back(lines[i]);
    }
    write_ini_sections(EXCLUDES_FILE, new_lines);
    std::cout << "Exclude path removed if matched." << std::endl;
}

// 예외 파일 목록 출력 함수
void ConfigurationManager::fim_show_exclude_path() {
    std::vector<std::string> lines = read_ini_sections(EXCLUDES_FILE);
    for (const auto& line : lines) {
        std::cout << line << std::endl;
    }
}
