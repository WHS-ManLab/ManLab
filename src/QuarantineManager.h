#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "DBManager.h"

struct ScanInfo 
{
    std::string path;   // 원본 파일 경로
    std::string cause;  // 탐지 원인
    std::string name;   // 악성코드 이름 또는 해시
    long long   size;   // 바이트 단위
};


class QuarantineManager
{
public:
    QuarantineManager(const std::vector<ScanInfo>& infos);

    // 격리 실행
    void Run();

    // 성공 여부 목록 반환
    const std::vector<bool>& GetIsQuarantineSuccess() const;

private:
    std::vector<ScanInfo> mScanInfo;               // 격리 대상 정보 목록
    std::vector<bool> mIsQuarantineSuccess;        // 격리 성공 여부

    StorageQuarantine* mStorage = nullptr;         // DBManager 싱글톤에서 획득

    static unsigned char sEncryptionKey;

    bool applySimpleXOREncryption(const std::string& filePath); // 악성코드 파일 암호화 함수
    std::string getCurrentDateTime() const; // 현재 시간 문자열로 가져오는 함수
};
