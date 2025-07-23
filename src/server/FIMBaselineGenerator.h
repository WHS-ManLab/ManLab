#pragma once
#include <string>
#include <ostream>
#include <set>
#include "DBManager.h"

class BaselineGenerator {
public:
    // 생성자: INI 경로와 DB 경로를 전달받아 저장
    BaselineGenerator(const std::string& ini_path, const std::string& db_path);

    // 메인 실행 함수: baseline 생성 및 저장 (출력은 클라이언트로)
    void generate_and_store(std::ostream& out);

    // MD5 해시 계산 함수 (외부 사용 가능)
    static std::string compute_md5(const std::string& filepath);

    // 파일 경로와 해시값을 기반으로 BaselineEntry 생성
    BaselineEntry collect_metadata(const std::string& path, const std::string& md5);

private:
    std::string ini_path_;
    std::string db_path_;

    // 내부 실행: ini 파일 파싱 → 경로 순회 및 해시/메타데이터 저장
    void parse_ini_and_store(std::ostream& out);

    // 프로그래스바 바로 밑에 현재 파일명 출력
   //void PrintCurrentFileBelowProgress(std::ostream& out, const std::string& filePath);
};
