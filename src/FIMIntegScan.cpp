#include "FIMIntegScan.h"
#include "DBManager.h"
#include "FIMBaselineGenerator.h"
#include "Paths.h"

#include <iostream>
#include <filesystem>
#include <vector>
#include <sys/stat.h>
#include <ctime>

// spdlog 헤더 추가
#include "spdlog/spdlog.h"
#include "spdlog/sinks/rotating_file_sink.h"

// 기준선 DB와 현재 파일 상태를 비교하여 변조 여부 확인
// verbose가 true일 경우 상세 로그를 출력
void compare_with_baseline(bool verbose, std::ostream& out) {
    // BaselineGenerator 객체 생성 (collect_metadata 호출용)
    BaselineGenerator generator(PATH_FIM_INTEG_INI, PATH_BASELINE_DB);

    auto& baseline_storage = DBManager::GetInstance().GetBaselineStorage();  // 기준선 DB 접근
    auto& modified_storage = DBManager::GetInstance().GetModifiedStorage();  // 수정된 DB 접근

    std::vector<BaselineEntry> entries;
    try {
        entries = baseline_storage.get_all<BaselineEntry>(); // DB에서 모든 기준선 항목 불러오기
    } catch (const std::exception& e) {
        if (verbose)
            out << "[ERROR] 기준선 DB 조회 실패: " << e.what() << std::endl;
        spdlog::error("기준선 DB 조회 실패: {}", e.what()); // 에러 로깅
        return;
    }

    if (entries.empty()) {
        if (verbose)
            out << "[INFO] 기준선에 등록된 항목이 없습니다.\n";
        spdlog::info("기준선에 등록된 항목이 없습니다."); // 정보 로깅
        return;
    }

    for (const auto& entry : entries) {
        const std::string& path = entry.path;

        if (!std::filesystem::exists(path)) {
            if (verbose) {
                out << "[WARN] 파일 없음: " << path << std::endl;
                spdlog::warn("기준선 파일 없음: {}", path); // 경고 로깅
            }
            continue;
        }

        // 현재 파일의 MD5 해시값 계산
        std::string current_hash = BaselineGenerator::compute_md5(path);
        if (current_hash.empty()) {
            if (verbose) {
                out << "[ERROR] 해시 계산 실패: " << path << std::endl;
                spdlog::error("해시 계산 실패: {}", path); // 에러 로깅
            }
            continue;
        }

        // 현재 파일의 메타데이터 수집
        BaselineEntry current_entry;
        try {
            current_entry = generator.collect_metadata(path, current_hash);
        } catch (const std::exception& e) {
            out << "[ERROR] 메타데이터 수집 실패: " << e.what() << std::endl;
            spdlog::error("메타데이터 수집 실패: {}", e.what()); // 에러 로깅
            continue;
        }

        // 변조 여부 판단 (MD5 또는 메타데이터 중 하나라도 다르면 변조)
        bool tampered =
            current_entry.md5        != entry.md5  ||
            current_entry.permission != entry.permission ||
            current_entry.uid        != entry.uid ||
            current_entry.gid        != entry.gid ||
            current_entry.ctime      != entry.ctime ||
            current_entry.mtime      != entry.mtime ||
            current_entry.size       != entry.size;

        if (!tampered) {
            if (verbose)
                out << "[OK] 일치: " << path << std::endl;
        } else {
            out << "[ALERT] 변조 감지: " << path << std::endl;
            out << "  - 기준선 MD5: " << entry.md5 << "\n";
            out << "  - 현재 MD5:   " << current_entry.md5 << "\n";
            if (entry.permission != current_entry.permission)
                out << "  - 권한 변경: " << entry.permission << " → " << current_entry.permission << "\n";
            if (entry.uid != current_entry.uid || entry.gid != current_entry.gid)
                out << "  - 소유자 변경: UID " << entry.uid << " → " << current_entry.uid
                    << ", GID " << entry.gid << " → " << current_entry.gid << "\n";
            if (entry.ctime != current_entry.ctime)
                out << "  - 생성시간 변경: " << entry.ctime << " → " << current_entry.ctime << "\n";
            if (entry.mtime != current_entry.mtime)
                out << "  - 수정시간 변경: " << entry.mtime << " → " << current_entry.mtime << "\n";
            if (entry.size != current_entry.size)
                out << "  - 크기 변경: " << entry.size << " → " << current_entry.size << " bytes\n";

            try {
                // ModifiedEntry로만 저장 (BaselineEntry는 매핑 대상이 아님)
                modified_storage.replace(ModifiedEntry{path, current_hash});
            } catch (const std::exception& e) {
                out << "[ERROR] 변조 항목 저장 실패: " << e.what() << std::endl;
                spdlog::error("변조 항목 저장 실패: {}", e.what()); // 에러 로깅
            }
        }
    }

    if (verbose)
        out << "\n[SUCCESS] 무결성 검사 완료\n";
}
