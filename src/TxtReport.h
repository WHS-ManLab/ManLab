#pragma once

#include <string>

namespace manlab {

// 텍스트 리포트를 기록하는 클래스입니다.
// 지정된 파일명과 내용을 받아 실행 바이너리 기준 상위 경로의 /log 디렉터리에 파일을 생성하거나 내용 추가합니다.
class TxtReport final {
public:
    // fileName: 저장할 파일명 (확장자 포함)
    // content : 기록할 텍스트 내용
    static void WriteReport(const std::string& fileName,
                            const std::string& content);

private:
    // log 디렉터리까지의 전체 경로를 생성해 반환
    static std::string GetLogFilePath(const std::string& fileName);

    TxtReport() = delete;
};

} // namespace manlab