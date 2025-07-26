#include <ostream>

void PrintCommandHelp(std::ostream& out)
{
    out << "\033[1m======================== ManLab 도움말 ========================\033[0m\n\n";
   
    // [1] 명령어 목록
    out << "\033[1;34m[명령어 목록]\033[0m\n"
        << "  ManLab malscan                    : 악성코드 수동 검사 수행\n"
        << "  ManLab restore <filename>         : 격리된 파일 복원\n"
        << "  check_malreport                   : 최근 악성코드 검사 리포트 20개 출력\n"
        << "  ManLab baseline                   : 무결성 검사 수행\n"
        << "  ManLab integscan                  : 기준 상태 수집\n"
        << "  ManLab check_baseline             : 수집된 베이스라인 출력\n"
        << "  ManLab check_integscan            : 무결성 검사 결과 출력\n"
        << "  ManLab man                        : 이 도움말 출력\n\n";

    // [2] 명령어 설명
    out << "\033[1;34m[명령어 사용]\033[0m\n"
        << "  malscan\n"
        << "    악성코드 검사 수행. 검사 대상, 제외 경로, 파일 크기 제한은\n"
        << "    MalScanConfig.ini 에서 설정합니다.\n\n"

        << "  restore <filename>\n"
        << "    격리된 악성 파일을 검역소에서 복구합니다.\n"
        << "    원본 파일 이름이 아닌 quarantine/ 내의 파일 이름을 입력해야 합니다.\n"
        << "    복구된 파일은 원래 존재하던 경로에 복구됩니다.\n\n"

        << "  check_malreport\n"
        << "    최근 수행된 악성코드 검사 리포트 중 20개를 최신순으로 출력합니다.\n\n"

        << "  baseline\n"
        << "    FIMIntegScan.ini 설정을 기반으로 지정된 경로의 파일에 대해\n"
        << "    현재 해시, 권한, UID/GID, 시간 정보를 수집하여 기준(Baseline)으로 저장합니다.\n\n"

        << "  integscan\n"
        << "    baseline 명령으로 저장된 기준값과 현재 파일 상태를 비교하여\n"
        << "    무결성 변조 여부를 검사합니다. \n\n"

        << "  check_baseline\n"
        << "    baseline으로 저장된 파일 목록 및 해시/속성 정보를 출력합니다.\n\n"

        << "  check_integscan\n"
        << "    무결성 검사 결과 (modifiedhash.db)에 기록된 변조된 파일 목록을 출력합니다.\n\n";
    
    // [3] INI 파일 설명
    out << "\033[1;34m[설정 파일]\033[0m\n"
        << "  FIMConfig.ini              : 실시간 파일 변경 감시 설정\n"
        << "  FIMIntegScan.ini           : 무결성 검사 대상/제외 설정\n"
        << "  MalScanConfig.ini          : 악성코드 검사 대상 및 제외, 용량 제한\n"
        << "  ManLabconf.ini             : 재부팅 시 프로그램 자동 실행 설정\n"
        << "  ReportConfig.ini           : 리포트 주기 및 이메일 전송 설정\n"
        << "  ScanSchedul.ini            : 예약 검사 시간 및 주기 설정\n"
        << "                              → 자세한 형식은 각 INI 파일 내 주석 참고\n\n";

    // [4] 주요 파일 및 디렉토리
    out << "\033[1;34m[주요 파일 및 디렉토리]\033[0m\n"
        << "  /root/ManLab/conf/           : 설정 파일 디렉토리\n"
        << "  /root/ManLab/rules/          : YARA 룰 파일\n"
        << "  /root/ManLab/quarantine/     : 격리된 파일 저장소\n"
        << "  /root/ManLab/logs/manlab.log : 시스템 로그 파일\n\n";

    // [5] 시스템 부팅
    out << "\033[1;34m[시스템]\033[0m\n"
        << "  재부팅시 제품 실행  : ManLabconf.ini에서 설정 가능합니다\n"
        << "  제품 삭제          : 제공되는 uninstall.sh 실행으로 가능합니다\n\n";

    out << "\033[1m===============================================================\033[0m\n";
}