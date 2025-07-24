#include "FimCommandHandler.h"
#include "FIMBaselineGenerator.h"
#include "FIMIntegScan.h"
#include "DBManager.h"
#include "Paths.h"
#include <iostream>
#include <ostream>
#include "spdlog/spdlog.h" // spdlog 헤더
#include "spdlog/sinks/rotating_file_sink.h" // spdlog 회전 파일 싱크 헤더

void FimCommandHandler::IntScan(std::ostream& out) {

    spdlog::info("해시값 무결성 검사 시작."); // info 레벨 로그

    out << "해시값 무결성 검사 실행중..." << std::endl;
    
    try {
        compare_with_baseline(true, out);
        spdlog::info("해시값 무결성 검사 완료."); // info 레벨 로그
    } catch (const std::exception& e) {
        spdlog::error(" 무결성 검사 중 오류 발생: {}", e.what()); // error 레벨 로그
        out << "[ERROR] 무결성 검사 중 오류 발생: " << e.what() << std::endl;
    }
}

void FimCommandHandler::BaselineGen(std::ostream& out) {
    spdlog::info("Baseline 해시값 생성 시작."); // info 레벨 로그
    out << "baseline 해시값 생성중...\n" << std::endl;

    const std::string ini_path = PATH_FIM_INTEG_INI;  // INI 파일 경로
    const std::string db_path  = PATH_BASELINE_DB;      // DB 저장 경로

    try {
        BaselineGenerator generator(ini_path, db_path);
        generator.generate_and_store(out);
        out << "[SUCCESS] Baseline 생성 완료\n";
        spdlog::info("Baseline 생성 완료."); // info 레벨 로그
    } catch (const std::exception& e) {
        out << "[ERROR] Baseline 생성 실패: " << e.what() << '\n';
        spdlog::error("Baseline 생성 실패: {}", e.what()); // error 레벨 로그
    }

}
void FimCommandHandler::PrintBaseline(std::ostream& out) {
    spdlog::info("Baseline DB 내용 출력 시작."); // info 레벨 로그
    out << "\n[INFO] Baseline DB 내용 출력:\n" << std::endl;
    auto& storage = DBManager::GetInstance().GetBaselineStorage();

    try {
        auto all_entries = storage.get_all<BaselineEntry>();
        if (all_entries.empty()) {
            out << "DB에 저장된 해시값이 없습니다." << std::endl;
            spdlog::info("Baseline DB에 저장된 해시값이 없습니다."); // info 레벨 로그
            return;
        }

        spdlog::info("Baseline DB에서 {}개의 항목 발견.", all_entries.size()); // info 레벨 로그 (항목 개수)
        for (const auto& entry : all_entries) {
            out << "Path:       " << entry.path << "\n"
                << "MD5:        " << entry.md5 << "\n"
                << "Permission: " << entry.permission << "\n"
                << "UID:        " << entry.uid << "\n"
                << "GID:        " << entry.gid << "\n"
                << "CTime:      " << entry.ctime << "\n"
                << "MTime:      " << entry.mtime << "\n"
                << "Size:       " << entry.size << " bytes\n"
                << "-------------------------------------\n";
        }
        spdlog::info("Baseline DB 내용 출력 완료."); // info 레벨 로그
    } catch (const std::exception& e) {
        out << "[ERROR] DB 조회 중 오류 발생: " << e.what() << std::endl;
        spdlog::error("Baseline DB 조회 중 오류 발생: {}", e.what()); // error 레벨 로그
    }
}
void FimCommandHandler::PrintIntegscan(std::ostream& out) {
    spdlog::info("무결성 검사 결과 출력 시작."); // info 레벨 로그
    out << "[INFO] 무결성 검사 결과 출력 중...\n" << std::endl;

    // 변조된 항목 DB 불러오기
    auto& modified_storage = DBManager::GetInstance().GetModifiedStorage();

    try {
        auto modified_entries = modified_storage.get_all<ModifiedEntry>();

        if (modified_entries.empty()) {
            out << "[INFO] 변조된 파일이 없습니다.\n";
            spdlog::info("변조된 파일이 발견되지 않았습니다."); // info 레벨 로그
        } else {
            out << "\n[ALERT] 변조된 파일 목록:\n";
            spdlog::warn("{}개의 변조된 파일 발견!", modified_entries.size()); // warn 레벨 로그
            for (const auto& entry : modified_entries) {
                out << "Path: " << entry.path << "\n"
                    << "Current MD5: " << entry.current_md5 << "\n"
                    << "Current Permission: " << entry.current_permission << "\n"
                    << "Current UID:        " << entry.current_uid << "\n"
                    << "Current GID:        " << entry.current_gid << "\n"
                    << "Current CTime:      " << entry.current_ctime << "\n"
                    << "Current MTime:      " << entry.current_mtime << "\n"
                    << "Current Size:       " << entry.current_size << " bytes\n"
                    << "-------------------------------------\n";
            }
            out << "\n[ALERT] " << modified_entries.size() << "개의 변조된 파일 발견!\n";
            spdlog::info("변조된 파일 목록 출력 완료."); // info 레벨 로그
        }

    } catch (const std::exception& e) {
        out << "[ERROR] modifiedhash.db 조회 실패: " << e.what() << std::endl;
        spdlog::error("modifiedhash.db 조회 실패: {}", e.what()); // error 레벨 로그
    }
}