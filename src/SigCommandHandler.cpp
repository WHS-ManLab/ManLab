#include "SigCommandHandler.h"
#include "MalwareScan.h"
#include "RestoreManager.h"

#include <iostream>
#include <chrono>

namespace sig {

void MalScan() {
    using namespace std::chrono;
    std::cout << "Executing malware scan..." << std::endl;

    auto start = system_clock::now();

    MalwareScan malscan;
    malscan.run();

    auto end = system_clock::now();
    auto elapsed = duration_cast<seconds>(end - start);
    int minutes = elapsed.count() / 60;
    int seconds = elapsed.count() % 60;

    auto now = system_clock::to_time_t(start);
    std::tm* localTime = std::localtime(&now);
    char timeBuffer[20];
    std::strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", localTime);

    std::cout << "\n[Scan Type] Manual" << std::endl;
    std::cout << "[Scan Time] " << timeBuffer << std::endl;
    std::cout << "[Scanned Path] " << malscan.getScanRootPath() << std::endl;

    const auto& results = malscan.getDetectionResults();
    const auto& quarantined = malscan.getQuarantineResults();

    std::cout << "\n[Detected Files]" << std::endl;
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& info = results[i];
        bool isQuarantined = (i < quarantined.size()) ? quarantined[i] : false;

        std::string prefix = isQuarantined ? "\033[31m" : "";
        std::string suffix = isQuarantined ? "\033[0m" : "";

        std::cout << prefix;
        std::cout << "  " << (i + 1) << ". File: " << info.path << std::endl;

        // 감지 시간
        auto t = system_clock::now();
        std::time_t dt = system_clock::to_time_t(t);
        std::tm* tmPtr = std::localtime(&dt);
        char detectTime[20];
        std::strftime(detectTime, sizeof(detectTime), "%Y-%m-%d %H:%M:%S", tmPtr);
        std::cout << "     Detected At: " << detectTime << std::endl;

        std::cout << "     Quarantine: " << (isQuarantined ? "Success" : "Failed") << std::endl;
        std::cout << "     Reason: " << info.cause << " match" << std::endl;

        if (info.cause == "yara") {
            std::cout << "     Rule Name: " << info.name << std::endl;
        } else {
            std::cout << "     Hash: " << info.name.substr(0, 24) << "..." << std::endl;
            std::cout << "     Malware Name: " << info.name << std::endl;
        }
        std::cout << "     Original Size: " << info.size << " bytes" << std::endl;
        std::cout << suffix;
    }

    std::cout << "\n[Summary]" << std::endl;
    std::cout << "Scanned Files: " << malscan.getTotalScannedFiles() << std::endl;
    std::cout << "Infected Files: " << malscan.getDetectedFiles() << std::endl;
    std::cout << "Quarantined Files: " << malscan.getQuarantinedFiles() << std::endl;
    std::cout << "Elapsed Time: " << std::setfill('0') << std::setw(2) << minutes
              << ":" << std::setw(2) << seconds << std::endl;
}

void Restore(const std::string& filename) {
    std::cout << "Restoring file: " << filename << std::endl;

    RestoreManager restorer(filename);
    restorer.run();

    if (restorer.isSuccess()) {
        std::cout << "[+] 복구 성공: " << filename << std::endl;
    } else {
        std::cerr << "[-] 복구 실패: " << filename << std::endl;
    }
}

} // namespace sig