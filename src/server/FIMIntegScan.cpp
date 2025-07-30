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
#include "indicator.hpp"  // ProgressBar

// 기준선 DB와 현재 파일 상태를 비교하여 무결성 검사 수행
// bVerbose=true: 콘솔 출력 활성화
void CompareWithBaseline(bool bVerbose, std::ostream& out)
{
    // BaselineGenerator 객체 생성 (CollectMetadata 호출용)
    BaselineGenerator generator(PATH_FIM_INTEG_INI, PATH_BASELINE_DB);

    auto& baselineStorage = DBManager::GetInstance().GetBaselineStorage();  // 기준선 DB 접근
    auto& modifiedStorage = DBManager::GetInstance().GetModifiedStorage();  // 수정된 DB 접근

    std::vector<BaselineEntry> entries;
    try
    {
        entries = baselineStorage.get_all<BaselineEntry>(); // DB에서 모든 기준선 항목 불러오기
    }
    catch (const std::exception& e)
    {
        if (bVerbose)
            out << "[ERROR] 기준선 DB 조회 실패: " << e.what() << std::endl;
        spdlog::error("기준선 DB 조회 실패: {}", e.what());
        return;
    }

    if (entries.empty())
    {
        if (bVerbose)
            out << "[INFO] 기준선에 등록된 항목이 없습니다.\n";
        spdlog::info("기준선에 등록된 항목이 없습니다.");
        return;
    }

    indicators::show_console_cursor(false);

    indicators::ProgressBar bar{
        indicators::option::BarWidth{50},
        indicators::option::Start{"["},
        indicators::option::Fill{"="},
        indicators::option::Lead{">"},
        indicators::option::Remainder{"-"},
        indicators::option::End{"]"},
        indicators::option::ShowPercentage{true},
        indicators::option::ShowElapsedTime{true},
        indicators::option::ShowRemainingTime{true},
        indicators::option::Stream{out}
    };

    size_t processed = 0;
    size_t total = entries.size();

    for (const auto& entry : entries)
    {
        const std::string& path = entry.path;

        if (!std::filesystem::exists(path))
        {
            if (bVerbose)
                spdlog::warn("기준선 파일 없음: {}", path);
            bar.set_progress(++processed * 100.0f / total);
            continue;
        }

        std::string currentHash = BaselineGenerator::ComputeMd5(path);
        if (currentHash.empty())
        {
            if (bVerbose)
                spdlog::error("해시 계산 실패: {}", path);
            bar.set_progress(++processed * 100.0f / total);
            continue;
        }

        BaselineEntry currentEntry;
        try
        {
            currentEntry = generator.CollectMetadata(path, currentHash);
        }
        catch (const std::exception& e)
        {
            if (bVerbose)
                spdlog::error("메타데이터 수집 실패: {} ({})", path, e.what());
            bar.set_progress(++processed * 100.0f / total);
            continue;
        }

        bool tampered =
            currentEntry.md5        != entry.md5  ||
            currentEntry.permission != entry.permission ||
            currentEntry.uid        != entry.uid ||
            currentEntry.gid        != entry.gid ||
            currentEntry.ctime      != entry.ctime ||
            currentEntry.mtime      != entry.mtime ||
            currentEntry.size       != entry.size;

        if (tampered)
        {
            try
            {
                modifiedStorage.replace(ModifiedEntry{
                    path,
                    currentHash,
                    currentEntry.permission,
                    currentEntry.uid,
                    currentEntry.gid,
                    currentEntry.ctime,
                    currentEntry.mtime,
                    currentEntry.size
                });
            }
            catch (const std::exception& e)
            {
                if (bVerbose)
                    out << "[ERROR] 변조 항목 저장 실패: " << e.what() << std::endl;
                spdlog::error("변조 항목 저장 실패: {}", e.what());
            }
        }

        bar.set_progress(++processed * 100.0f / total);
    }

    out << std::endl;
    indicators::show_console_cursor(true);

    auto alerts = DBManager::GetInstance().GetModifiedStorage().get_all<ModifiedEntry>();
    for (const auto& m : alerts)
    {
        BaselineEntry old = DBManager::GetInstance().GetBaselineStorage().get<BaselineEntry>(m.path);
        BaselineEntry curr = generator.CollectMetadata(m.path, m.current_md5);

        out << "[ALERT] 변조 감지: " << m.path << std::endl;
        out << "  - 기준선 MD5: " << old.md5 << std::endl;
        out << "  - 현재 MD5:   " << curr.md5 << std::endl;
        if (old.permission != curr.permission)
            out << "  - 권한 변경: " << old.permission << " → " << curr.permission << std::endl;
        if (old.uid != curr.uid || old.gid != curr.gid)
            out << "  - 소유자 변경: UID " << old.uid << " → " << curr.uid
                << ", GID " << old.gid << " → " << curr.gid << std::endl;
        if (old.ctime != curr.ctime)
            out << "  - 생성시간 변경: " << old.ctime << " → " << curr.ctime << std::endl;
        if (old.mtime != curr.mtime)
            out << "  - 수정시간 변경: " << old.mtime << " → " << curr.mtime << std::endl;
        if (old.size != curr.size)
            out << "  - 크기 변경: " << old.size << " → " << curr.size << " bytes" << std::endl;

        out << std::endl;
    }

    if (bVerbose)
        out << "\n[SUCCESS] 무결성 검사 완료\n";
}
