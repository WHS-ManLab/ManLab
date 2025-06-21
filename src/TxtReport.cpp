#include "TxtReport.h"
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <unistd.h>

namespace manlab {

namespace fs = std::filesystem;

// 실행 파일이 위치한 디렉터리를 반환
std::string TxtReport::GetLogFilePath(const std::string& fileName) {
    char pathBuf[4096];
    ssize_t count = readlink("/proc/self/exe", pathBuf, sizeof(pathBuf));
    if (count == -1 || count >= static_cast<ssize_t>(sizeof(pathBuf))) {
        // TODO: 로그 처리 
        return "";
    }
    pathBuf[count] = '\0';

    fs::path exeDir = fs::path(pathBuf).parent_path(); // 예: /Manlab/src/
    fs::path logDir = exeDir.parent_path() / "log";    // 예: /Manlab/log/

    if (!fs::exists(logDir)) {
        fs::create_directories(logDir);
    }
    return (logDir / fileName).string();
}


void TxtReport::WriteReport(const std::string& fileName,
                            const std::string& content) {
    std::string fullPath = GetLogFilePath(fileName);

    std::ofstream ofs(fullPath, std::ios::app);
    if (!ofs) {
        // TODO: 로그 처리 
        return "";
    }

    ofs << content;
    // 마지막에 개행이 없으면 추가
    if (content.empty() || content.back() != '\n') {
        ofs << '\n';
    }
}

} // namespace manlab