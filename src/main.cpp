#include "baseline_generator.h"

int main() {
    std::string ini_path = "test.ini";
    std::string db_path = "baseline.db";

    BaselineGenerator generator(ini_path, db_path);
    generator.generate_and_store();

    return 0;
}

