#include "DaemonBase.h"
#include <csignal>
#include <unistd.h>
#include <cstdlib>
#include <iostream>
#include <fcntl.h>
#include <sys/stat.h>

volatile bool DaemonBase::running = true;

// 핸들러 등록
// 데몬 종료 후 정리할 것이 있다면 running 루프 아래에서 정리
// SIGKILL 등 의도하지 않은 종료 시 실행이 안 될 수 있음에 유의
void DaemonBase::handleSignals() {
    signal(SIGTERM, [](int){ running = false; });
    signal(SIGINT,  [](int){ running = false; });
}

// 데몬화 절차
void DaemonBase::daemonize() {
    if (setsid() < 0) exit(1);          // 세션 분리
    chdir("/");                         // 루트 디렉토리 이동  
    umask(0);                           // 파일 생성 마스크 제거

    // 표준 입출력 차단
    freopen("/dev/null", "r", stdin);    
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);
}