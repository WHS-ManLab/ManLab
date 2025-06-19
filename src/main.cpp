#include <string>
#include "baseline_generator.h"
#include "compare_with_baseline.h"

int main() {
    std::string ini_path = "test.ini";
    std::string db_path = "baseline.db";

    BaselineGenerator generator(ini_path, db_path);
    generator.generate_and_store();
    
    compare_with_baseline(db_path);
    return 0;
}

