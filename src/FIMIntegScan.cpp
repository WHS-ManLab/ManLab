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
void CompareWithBaseline(bool bVerbose)
{
    auto& baselineStorage = DBManager::GetInstance().GetBaselineStorage(); // 기준선 DB에 접근
    auto& modifiedStorage = DBManager::GetInstance().GetModifiedStorage(); // 변조된 파일 정보를 기록할 테이블에 접근

    // BaselineEntry 객체들을 저장할 벡터 선언
    std::vector<BaselineEntry> entries;
    try
    {
        entries = baselineStorage.get_all<BaselineEntry>(); // 기준선 DB에서 모든 항목 불러옴
    }
    catch (const std::exception& e)
    {
        if (bVerbose)
        {
            std::cerr << "[ERROR] 기준선 DB 조회 실패: " << e.what() << std::endl;
        }
        return;
    }

    if (entries.empty())
    {
        if (bVerbose)
        {
            std::cout << "[INFO] 기준선에 등록된 항목이 없습니다.\n";
        }
        return;
    }

    // BaselineEntry들을 하나씩 순회
    for (const auto& entry : entries)
    {
        const std::string& path = entry.path;

        if (!std::filesystem::exists(path))
        {
            if (bVerbose)
            {
                std::cout << "[WARN] 파일 없음: " << path << std::endl;
            }
            continue;
        }

        // 현재 파일의 해시 및 메타데이터 수집
        std::string currentHash = BaselineGenerator::ComputeMd5(path);
        BaselineEntry currentMeta;

        try
        {
            currentMeta = BaselineGenerator::CollectMetadata(path, currentHash);
        }
        catch (const std::exception& e)
        {
            if (bVerbose)
            {
                std::cout << "[ERROR] 메타데이터 수집 실패: " << path << " - " << e.what() << std::endl;
            }
            continue;
        }

        // 기준선과 현재 상태 비교
        bool bModified = false;

        // 항목별로 하나라도 다르면 bModified = true
        if (entry.md5 != currentMeta.md5) bModified = true;
        if (entry.permission != currentMeta.permission) bModified = true;
        if (entry.uid != currentMeta.uid) bModified = true;
        if (entry.gid != currentMeta.gid) bModified = true;
        if (entry.ctime != currentMeta.ctime) bModified = true;
        if (entry.mtime != currentMeta.mtime) bModified = true;
        if (entry.size != currentMeta.size) bModified = true;

        if (!bModified)
        {
            if (bVerbose)
            {
                std::cout << "[OK] 일치: " << path << std::endl;
            }
        }
        else
        {
            std::cout << "[ALERT] 변조 감지: " << path << std::endl;

            if (entry.md5 != currentMeta.md5)
            {
                std::cout << "  - MD5 변경됨\n    기준: " << entry.md5 << "\n    현재: " << currentMeta.md5 << std::endl;
            }

            if (entry.permission != currentMeta.permission)
            {
                std::cout << "  - 권한 변경됨\n    기준: " << entry.permission << "\n    현재: " << currentMeta.permission << std::endl;
            }

            if (entry.uid != currentMeta.uid || entry.gid != currentMeta.gid)
            {
                std::cout << "  - 소유자 변경됨\n    기준: UID=" << entry.uid << ", GID=" << entry.gid
                          << "\n    현재: UID=" << currentMeta.uid << ", GID=" << currentMeta.gid << std::endl;
            }

            if (entry.ctime != currentMeta.ctime)
            {
                std::cout << "  - 생성시간 변경됨\n    기준: " << entry.ctime << "\n    현재: " << currentMeta.ctime << std::endl;
            }

            if (entry.mtime != currentMeta.mtime)
            {
                std::cout << "  - 수정시간 변경됨\n    기준: " << entry.mtime << "\n    현재: " << currentMeta.mtime << std::endl;
            }

            if (entry.size != currentMeta.size)
            {
                std::cout << "  - 크기 변경됨\n    기준: " << entry.size << " bytes\n    현재: " << currentMeta.size << " bytes" << std::endl;
            }

            try
            {
                modifiedStorage.replace(ModifiedEntry{path, currentMeta.md5});
            }
            catch (const std::exception& e)
            {
                std::cerr << "[ERROR] 변조 항목 저장 실패: " << e.what() << std::endl;
            }
        }
    }

    if (bVerbose)
    {
        std::cout << "\n[SUCCESS] 무결성 검사 완료\n";
    }
}
