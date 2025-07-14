#include "FIMIntegScan.h"
#include "DBManager.h"
#include "FIMBaselineGenerator.h"

#include <iostream>
#include <filesystem>
#include <vector>

//기준선 DB와 현재 파일 상태를 비교하여 변조 여부 확인
//verbose가 true일 경우 상세 로그를 출력
void compare_with_baseline(bool verbose, std::ostream& out) {
    auto& baseline_storage = DBManager::GetInstance().GetBaselineStorage();  // 기준선 DB 접근
    auto& modified_storage = DBManager::GetInstance().GetModifiedStorage();  // 수정된 DB 접근

    std::vector<BaselineEntry> entries;
    try {
        entries = baseline_storage.get_all<BaselineEntry>(); // DB에서 모든 기준선 항목 불러오기
    } catch (const std::exception& e) {
        if (verbose)
            out << "[ERROR] 기준선 DB 조회 실패: " << e.what() << std::endl;
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

        //현재 파일의 MD5 해시값 계산
        std::string current_hash = BaselineGenerator::compute_md5(path);
        if (current_hash.empty()) {
            if (verbose)
                std::cout << "[ERROR] 해시 계산 실패: " << path << std::endl;
            continue;
        }

        if (current_hash == stored_hash) {
            if (verbose)
                out << "[OK] 일치: " << path << std::endl;
        } else {
            out << "[ALERT] 변조 감지: " << path << std::endl;
            out << "  - 기준선: " << stored_hash << std::endl;
            out << "  - 현재값: " << current_hash << std::endl;

            try {
                modified_storage.replace(ModifiedEntry{path, current_hash});  // 변조된 항목 저장
            } catch (const std::exception& e) {
                out << "[ERROR] 변조 항목 저장 실패: " << e.what() << std::endl;
            }
        }
    }

    if (verbose)
        out << "\n[SUCCESS] 무결성 검사 완료\n";
} 

