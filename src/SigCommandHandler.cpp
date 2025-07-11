#include "SigCommandHandler.h"
#include "MalwareScan.h"
#include "RestoreManager.h"
#include "DBManager.h"

#include <iostream>
#include <chrono>
#include <iomanip>  // std::setw
#include <tuple>    // std::tie

namespace sig 
{

void MalScan(std::ostream& out)
{
    using namespace std;
    using namespace std::chrono;

    out << "Executing malware scan..." << endl;

    MalwareScan malscan;
    malscan.Init();
    malscan.Run(&out);
    malscan.PrintReport(out);
    malscan.SaveReportToDB();
}

void Restore(const std::string& filename, std::ostream& out)
{
    using namespace std;
    out << "Restoring file: " << filename << endl;

    RestoreManager restorer(filename);
    restorer.Run();

    if (restorer.IsSuccess())
    {
        out << "[+] 복구 성공: " << filename << endl;
    }
    else
    {
        out << "[-] 복구 실패: " << filename << endl;
    }
}

void CmdListReports(std::ostream& out)
{
    using namespace std;
    auto& storage = DBManager::GetInstance().GetScanReportStorage();

    vector<ScanReport> allReports = storage.get_all<ScanReport>();

    // 최신순 정렬
    std::sort(allReports.begin(), allReports.end(), [](const auto& a, const auto& b) {
        return a.id > b.id;
    });

    // 출력 헤더
    out << " ID |        DATE & TIME        | TYPE       | DETECTED\n";
    out << "----+---------------------------+------------+---------\n";

    size_t count = std::min<size_t>(20, allReports.size());
    for (size_t i = 0; i < count; ++i)
    {
        const auto& r = allReports[i];
        out << std::setw(3) << r.id << " | "
            << r.date << " | "
            << std::setw(10) << std::left << r.type << " | "
            << (r.detected ? "YES" : "NO") << "\n";
    }

    if (allReports.empty())
        out << "(no scan reports found)\n";
}

void CmdShowReport(int id, std::ostream& out)
{
    using namespace std;
    auto& storage = DBManager::GetInstance().GetScanReportStorage();
    auto ptr = storage.get_pointer<ScanReport>(id);

    if (!ptr)
    {
        out << "[!] 해당 ID(" << id << ")의 스캔 리포트가 존재하지 않습니다.\n";
        return;
    }

    const ScanReport& r = *ptr;

    out << "===== Scan Report #" << r.id << " =====\n";
    out << "Date     : " << r.date << "\n";
    out << "Type     : " << r.type << "\n";
    out << "Detected : " << (r.detected ? "YES" : "NO") << "\n";
    out << "------------------------------------------\n";
    out << r.report << "\n";
    out << "==========================================\n";
}

} // namespace sig
