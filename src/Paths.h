#pragma once

// IPC 경로
#define PATH_IPC                  "/root/ManLab/ipc"

// 소켓 PATH경로
#define PATH_SOCKET              "/root/ManLab/ipc/cmdsocket"

// PID 파일 경로
#define PATH_PID                  "/root/ManLab/pid"

// 해시 DB 경로
#define PATH_HASH_DB              "/root/ManLab/db/hash.db"

// 격리 메타데이터 DB 경로
#define PATH_QUARANTINE_DB        "/root/ManLab/db/quarantine.db"

// 로그 분석 결과 DB 경로
#define PATH_LOG_ANALYSIS_DB      "/root/ManLab/db/logAnalysisResult.db"

// 베이스라인 DB 경로
#define PATH_BASELINE_DB          "/root/ManLab/db/baseline.db"

// 변경 감지 DB 경로
#define PATH_MODIFIED_DB          "/root/ManLab/db/modifiedhash.db"

// 리포트 DB 경로
#define PATH_SCAN_REPORT_DB       "/root/ManLab/db/report.db"

// 해시 텍스트 초기화용 데이터
#define PATH_HASH_INIT_TXT        "/root/ManLab/malware/malware_hashes.txt"

// FIM 설정파일
#define PATH_FIM_CONFIG_INI       "/root/ManLab/conf/FIMConfig.ini"

// MALSCAN 설정파일
#define PATH_MALSCAN_CONFIG_INI   "/root/ManLab/conf/MalScanConfig.ini"

// 예약검사 설정파일
#define PATH_SCHEDUL_CONFIG_INI   "/root/ManLab/conf/ScanSchedul.ini"

// rules 디렉토리
#define PATH_RULES                "/root/ManLab/rules"

// 격리 디렉토리
#define PATH_QUARANTINE           "/root/ManLab/quarantine"

// 로그 디렉토리
#define PATH_LOG                  "/var/log/manlab.log"

// ruleset 디렉토리
#define PATH_RULESET              "/root/Manlab/conf/RsyslogRuleSet.yaml"

// ManLab 전체 config 파일
#define PATH_MANLAB_CONFIG_INI    "/root/ManLab/conf/ManLabconf.ini"