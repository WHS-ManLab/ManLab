#include "DaemonBase.h"

#include <csignal>
#include <unistd.h>
#include <cstdlib>
#include <iostream>
#include <fcntl.h>
#include <sys/stat.h>

// 데몬화 절차
void DaemonBase::daemonize()
{
    if (setsid() < 0) exit(1);          // 세션 분리
    chdir("/");                         // 루트 디렉토리 이동  
    umask(0);                           // 파일 생성 마스크 제거

    // 표준 입출력 차단
    freopen("/dev/null", "r", stdin);    
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);
}
