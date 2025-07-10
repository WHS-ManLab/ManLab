#include "FIMIntegScan.h"
#include "DBManager.h"
#include "FIMBaselineGenerator.h"

#include <iostream>
#include <filesystem>
#include <vector>
#include <sys/stat.h>
#include <sstream>
#include <iomanip>
#include <ctime>

// 기준선 DB와 현재 파일 상태를 비교하여 변조 여부 확인
void compare_with_baseline(bool verbose) {
    auto& baseline_storage = DBManager::GetInstance().GetBaselineStorage(); //기준선 DB에 접근
    auto& modified_storage = DBManager::GetInstance().GetModifiedStorage(); // 변조된 파일 정보를 기록할 테이블에 접근

    //BaselineEntry 객체들을 저장할 벡터 선언
    std::vector<BaselineEntry> entries;
    try {
        entries = baseline_storage.get_all<BaselineEntry>(); //기준선 DB에서 모든 항목 불러옴
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

    //BaselineEntry들을 하나씩 순회
    for (const auto& entry : entries) {
        const std::string& path = entry.path;

        if (!std::filesystem::exists(path)) {
            if (verbose)
                std::cout << "[WARN] 파일 없음: " << path << std::endl;
            continue;
        }

        // 현재 파일의 해시 및 메타데이터 수집
        std::string current_hash = BaselineGenerator::compute_md5(path);
        BaselineEntry current_meta;
        try {
            current_meta = BaselineGenerator::collect_metadata(path, current_hash);
        } catch (const std::exception& e) {
            if (verbose)
                std::cout << "[ERROR] 메타데이터 수집 실패: " << path << " - " << e.what() << std::endl;
            continue;
        }

        // 기준선과 현재 상태 비교
        bool modified = false;

        //항목별로 하나라도 다르면 modified = true
        if (entry.md5 != current_meta.md5) modified = true;
        if (entry.permission != current_meta.permission) modified = true;
        if (entry.uid != current_meta.uid) modified = true;
        if (entry.gid != current_meta.gid) modified = true;
        if (entry.ctime != current_meta.ctime) modified = true;
        if (entry.mtime != current_meta.mtime) modified = true;
        if (entry.size != current_meta.size) modified = true;

        if (!modified) {
            if (verbose)
                std::cout << "[OK] 일치: " << path << std::endl;
        } else {
            std::cout << "[ALERT] 변조 감지: " << path << std::endl;

            if (entry.md5 != current_meta.md5)
                std::cout << "  - MD5 변경됨\n    기준: " << entry.md5 << "\n    현재: " << current_meta.md5 << std::endl;

            if (entry.permission != current_meta.permission)
                std::cout << "  - 권한 변경됨\n    기준: " << entry.permission << "\n    현재: " << current_meta.permission << std::endl;

            if (entry.uid != current_meta.uid || entry.gid != current_meta.gid)
                std::cout << "  - 소유자 변경됨\n    기준: UID=" << entry.uid << ", GID=" << entry.gid
                          << "\n    현재: UID=" << current_meta.uid << ", GID=" << current_meta.gid << std::endl;

            if (entry.ctime != current_meta.ctime)
                std::cout << "  - 생성시간 변경됨\n    기준: " << entry.ctime << "\n    현재: " << current_meta.ctime << std::endl;

            if (entry.mtime != current_meta.mtime)
                std::cout << "  - 수정시간 변경됨\n    기준: " << entry.mtime << "\n    현재: " << current_meta.mtime << std::endl;

            if (entry.size != current_meta.size)
                std::cout << "  - 크기 변경됨\n    기준: " << entry.size << " bytes\n    현재: " << current_meta.size << " bytes" << std::endl;

            try {
                modified_storage.replace(ModifiedEntry{path, current_meta.md5});
            } catch (const std::exception& e) {
                std::cerr << "[ERROR] 변조 항목 저장 실패: " << e.what() << std::endl;
            }
        }
    }

    if (verbose)
        std::cout << "\n[SUCCESS] 무결성 검사 완료\n";
}
