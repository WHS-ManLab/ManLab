#include "ScanMalware.h"
#include "INIReader.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cstdlib>
#include <filesystem>

using namespace std;
namespace fs = std::filesystem;

// 문자열 앞뒤 공백 제거 유틸
string trim(const string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end = s.find_last_not_of(" \t\r\n");
    return (start == string::npos) ? "" : s.substr(start, end - start + 1);
}

int main() {
    // 1. 검사 루트 디렉토리: 홈 디렉토리
    string homeDir = getenv("HOME");
    string scanDir = homeDir;

    // 2. 예외 경로 및 최대 파일 크기 설정
    vector<string> exceptionDirs;
    long long maxFileSize = 50 * 1024 * 1024;  // 기본값: 50MB

    INIReader reader("../conf/SIGScanException.ini");
    if (reader.ParseError() == 0) {
        // [Exclude] 섹션 - 쉼표 구분된 경로 파싱
        string rawPaths = reader.Get("Exclude", "paths", "");
        istringstream ss(rawPaths);
        string path;
        while (getline(ss, path, ',')) {
            path = trim(path);
            if (!path.empty()) exceptionDirs.push_back(path);
        }

        // [Limit] 섹션 - 최대 파일 크기 (bytes 단위)
        maxFileSize = reader.GetInteger("Limit", "max_size", maxFileSize);
    } else {
        cerr << "Warning: Failed to parse SIGScanException.ini. Using default settings." << endl;
    }

    // 3. 악성코드 스캐너 실행
    ScanMalware scanner(scanDir, exceptionDirs, maxFileSize);
    scanner.run();

    // 4. 로그 디렉토리 생성 (없으면 자동 생성)
    fs::path logDir = homeDir + "/ManLab/logs";
    if (!fs::exists(logDir)) {
        try {
            fs::create_directories(logDir);
        } catch (const fs::filesystem_error& e) {
            cerr << "Failed to create log directory: " << e.what() << endl;
            return 1;
        }
    }

    // 5. 리포트 파일 생성
    string reportPath = logDir.string() + "/SIGScanReport.txt";
    ofstream report(reportPath);
    if (!report.is_open()) {
        cerr << "Failed to create report file at: " << reportPath << endl;
        return 1;
    }

    // 6. 리포트 내용 작성
    report << "=== Malware Scan Report ===" << endl;
    report << "Scan Directory      : " << scanDir << endl;
    report << "Total Scanned Files : " << scanner.getTotalScannedFiles() << endl;
    report << "Detected Files      : " << scanner.getDetectedFiles() << endl;
    report << "Quarantined Files   : " << scanner.getQuarantinedFiles() << endl;
    report << "Excluded Paths      : ";
    for (size_t i = 0; i < exceptionDirs.size(); ++i) {
        report << exceptionDirs[i];
        if (i != exceptionDirs.size() - 1) report << ", ";
    }
    report << endl << endl;

    const auto& results = scanner.getDetectionResults();
    for (const auto& r : results) {
        report << "[DETECTED]" << endl;
        report << "Path       : " << r.path << endl;
        report << "Cause      : " << r.cause << endl;
        report << "Name       : " << r.name << endl;
        report << "Size(bytes): " << r.size << endl;
        report << "Quarantined: " << (r.quarantined ? "Yes" : "No") << endl;
        report << "-------------------------" << endl;
    }

    report.close();
    cout << "Scan complete. Report saved to: " << reportPath << endl;
    return 0;
}