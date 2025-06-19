#include <string>
#include "compare_with_baseline.h"

int main() {
    std::string db_path = "baseline.db";  // 필요에 따라 경로 수정
    compare_with_baseline(db_path);
    return 0;
}
