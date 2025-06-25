<<<<<<< HEAD
#ifndef BASELINE_GENERATOR_H
#define BASELINE_GENERATOR_H

=======
#pragma once
>>>>>>> 8c5fb367890b21c1a6d2ad1fb2677f2a8cbca03f
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

<<<<<<< HEAD
#endif // BASELINE_GENERATOR_H
=======

>>>>>>> 8c5fb367890b21c1a6d2ad1fb2677f2a8cbca03f
