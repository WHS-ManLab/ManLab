#pragma once
#include <string>

namespace manlab::utils {

    // 문자열 양끝 공백 제거 함수
    std::string trim(const std::string& s);

    // 주석 제거 함수
    std::string stripComment(const std::string &s);
}