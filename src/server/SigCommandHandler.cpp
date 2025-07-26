#include "SigCommandHandler.h"
#include "MalwareScan.h"
#include "RestoreManager.h"
#include "DBManager.h"

#include <iostream>
#include <chrono>
#include <iomanip>  // std::setw
#include <tuple>    // std::tie
#include <spdlog/spdlog.h>



void SigCommandHandler::MalScan(std::ostream& out)
{
    using namespace std;
    using namespace std::chrono;

    out << "악성코드 탐지 중..." << endl;

    MalwareScan malscan;
    malscan.Init();
    malscan.Run(&out);
    spdlog::info("[malscan] 검사 완료, 리포트 출력 중");
    malscan.PrintReport(out);
    malscan.SaveReportToDB();
    spdlog::info("[malscan] 검사 결과 DB 저장 완료");
}


void SigCommandHandler::Restore(const std::string& filename, std::ostream& out) 
{
    using namespace std;
    out << "Restoring file: " << filename << endl;

    RestoreManager restorer(filename);
    restorer.Run();

    if (restorer.IsSuccess())
    {
        out << "[+] 복구 성공: " << filename << endl;
        spdlog::info("[restore] 복구 성공: {}", filename);
    }
    else
    {
        out << "[-] 복구 실패: " << filename << endl;
        spdlog::warn("[restore] 복구 실패: {}", filename);
    }
}

void SigCommandHandler::CmdShowRecentReports(std::ostream& out)
{
    using namespace std;
    auto& storage = DBManager::GetInstance().GetScanReportStorage();
    vector<ScanReport> allReports = storage.get_all<ScanReport>();

    // 최신순 정렬
    std::sort(allReports.begin(), allReports.end(), [](const auto& a, const auto& b) {
        return a.id > b.id;
    });

    size_t count = std::min<size_t>(10, allReports.size());
    if (count == 0)
    {
        out << "[INFO] 최근 검사 리포트가 존재하지 않습니다.\n";
        return;
    }

    for (size_t i = 0; i < count; ++i)
    {
        const auto& r = allReports[i];

        out << "\n\033[1m===== Scan Report #" << r.id << " =====\033[0m\n";
        out << "Date     : " << r.date << "\n";
        out << "Type     : " << r.type << "\n";
        out << "Detected : " << (r.detected ? "YES" : "NO") << "\n";
        out << "------------------------------------------\n";
        out << r.report << "\n";
        out << "==========================================\n";
    }
}