#pragma once
#include <string>
#include <ostream>

class BaselineGenerator {
public:
    BaselineGenerator(const std::string& ini_path, const std::string& db_path);
    void generate_and_store(std::ostream& out);

    static std::string compute_md5(const std::string& filepath);

private:
    std::string ini_path_;
    std::string db_path_;

    void parse_ini_and_store(std::ostream& out);
};