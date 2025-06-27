#pragma once
#include <string>

class BaselineGenerator {
public:
    BaselineGenerator(const std::string& ini_path, const std::string& db_path);
    void generate_and_store();

    static std::string compute_md5(const std::string& filepath);

private:
    std::string ini_path_;
    std::string db_path_;

    //std::string compute_md5(const std::string& filepath);
    void parse_ini_and_store();
};