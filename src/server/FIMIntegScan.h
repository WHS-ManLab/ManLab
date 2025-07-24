#pragma once

#include <ostream>

// 기준선 DB와 현재 파일 상태를 비교하여 무결성 검사 수행
// verbose=true: 콘솔 출력 활성화
void CompareWithBaseline(bool bVerbose, std::ostream& out);

