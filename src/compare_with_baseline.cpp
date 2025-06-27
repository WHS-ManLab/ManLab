#include "compare_with_baseline.h"
#include "DBManager.h"
#include "baseline_generator.h"

#include <iostream>
#include <filesystem>
#include <vector>

void compare_with_baseline(bool verbose) {
    auto& storage = DBManager::GetInstance().GetBaselineStorage();

    std::vector<BaselineEntry> entries;
    try {
        entries = storage.get_all<BaselineEntry>();
    } catch (const std::exception& e) {
        if (verbose)
            std::cerr << "[ERROR] 기준선 DB 조회 실패: " << e.what() << std::endl;
        return;
    }

    if (entries.empty()) {
        if (verbose)
            std::cout << "[INFO] 기준선에 등록된 항목이 없습니다.\n";
        return;
    }

    for (const auto& entry : entries) {
        const std::string& path = entry.path;
        const std::string& stored_hash = entry.md5;

        if (!std::filesystem::exists(path)) {
            if (verbose)
                std::cout << "[WARN] 파일 없음: " << path << std::endl;
            continue;
        }

        std::string current_hash = BaselineGenerator::compute_md5(path);
        if (current_hash.empty()) {
            if (verbose)
                std::cout << "[ERROR] 해시 계산 실패: " << path << std::endl;
            continue;
        }

        if (current_hash == stored_hash) {
            if (verbose)
                std::cout << "[OK] 일치: " << path << std::endl;
        } else {
            std::cout << "[ALERT] 변조 감지: " << path << std::endl;
            std::cout << "  - 기준선: " << stored_hash << std::endl;
            std::cout << "  - 현재값: " << current_hash << std::endl;
        }
    }

    if (verbose)
        std::cout << "\n[SUCCESS] 무결성 검사 완료\n";
}

