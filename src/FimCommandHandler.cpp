#include "FimCommandHandler.h"
#include "FIMBaselineGenerator.h"
#include "FIMIntegScan.h"
#include "DBManager.h"
#include <iostream>

namespace fim {


void IntScan() {
    std::cout << "해시값 무결성 검사 실행중...\n" << std::endl;
    
    compare_with_baseline(true);
}

void BaselineGen() {
    std::cout << "baseline 해시값 생성중...\n" << std::endl;
    const std::string ini_path = "/ManLab/conf/FIMConfig.ini";  // INI 파일 경로
    const std::string db_path  = "/ManLab/db/baseline.db";      // DB 저장 경로

    try {
        BaselineGenerator generator(ini_path, db_path);
        generator.generate_and_store();
        std::cout << "[SUCCESS] Baseline 생성 완료\n";
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Baseline 생성 실패: " << e.what() << '\n';
    }

}
void PrintBaseline() {
    std::cout << "\n[INFO] Baseline DB 내용 출력:\n" << std::endl;
    auto& storage = DBManager::GetInstance().GetBaselineStorage();

    try {
        auto all_entries = storage.get_all<BaselineEntry>();
        if (all_entries.empty()) {
            std::cout << "DB에 저장된 해시값이 없습니다." << std::endl;
            return;
        }

        for (const auto& entry : all_entries) {
            std::cout << "Path: " << entry.path << "\nMD5:  " << entry.md5 << "\n---" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] DB 조회 중 오류 발생: " << e.what() << std::endl;
    }
}
void PrintIntegscan() {
     std::cout << "[INFO] 무결성 검사 중...\n" << std::endl;

    // 변조된 항목 DB 불러오기
    auto& modified_storage = DBManager::GetInstance().GetModifiedStorage();

    try {
        auto modified_entries = modified_storage.get_all<ModifiedEntry>();

        if (modified_entries.empty()) {
            std::cout << "[INFO] 변조된 파일이 없습니다.\n";
        } else {
            std::cout << "\n[ALERT] 변조된 파일 목록:\n";
            for (const auto& entry : modified_entries) {
                std::cout << "Path: " << entry.path << "\n"
                          << "Current MD5: " << entry.current_md5 << "\n---\n";
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "[ERROR] modifiedhash.db 조회 실패: " << e.what() << std::endl;
    }
}


}