#pragma once
#include <string>
#include <ostream>
#include <set>
#include "DBManager.h"

class BaselineGenerator
{
public:
    // 생성자: INI 경로와 DB 경로를 전달받아 저장
    BaselineGenerator(const std::string& iniPath, const std::string& dbPath);

    // 메인 실행 함수: baseline 생성 및 저장 (출력은 클라이언트로)
    void GenerateAndStore(std::ostream& out);

    // MD5 해시 계산 함수 (외부 사용 가능)
    static std::string ComputeMd5(const std::string& filePath);

    // 파일 경로와 해시값을 기반으로 BaselineEntry 생성
    BaselineEntry CollectMetadata(const std::string& path, const std::string& md5);

private:
    std::string mIniPath;
    std::string mDbPath;

    // 내부 실행: ini 파일 파싱 → 경로 순회 및 해시/메타데이터 저장
    void ParseIniAndStore(std::ostream& out);
};