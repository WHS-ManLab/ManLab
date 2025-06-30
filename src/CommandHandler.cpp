#include "CommandHandler.h"
#include <getopt.h>
#include <iostream>
#include <stdexcept>
#include <filesystem>
#include <unistd.h>  // for getopt_long
#include "FimCommandHandler.h"
#include "SigCommandHandler.h"
#include "RealtimeMonitorDaemon.h"
#include "LogDaemon.h"
#include "ScheduledScanDaemon.h"
#include "DBManager.h"

namespace fs = std::filesystem;

CommandHandler::CommandHandler(int argc, char** argv)
    : mArgc(argc), mArgv(argv)
{
    parseOptions(); // 옵션 파싱 시 예외 throw
}


// getopt_long()를 이용해 CLI 옵션을 분리-저장하는 함수
// * 새 옵션을 추가하려는 분은 **① longOptions[], ② optString,
//   ③ switch-case**—세 군데만 수정하시면 됩니다.
void CommandHandler::parseOptions()
{
    optind = 0;
    int  optionChar;          // getopt_long()이 반환하는 문자(또는 0/-1)
    int  optionIndex = 0;     // longOptions[]에서 몇 번째 항목인지

    //-----------------------------------------------------------
    // ① longOptions[] : “--long-name” ↔ ‘-s’ 매핑 테이블
    //    • { "옵션이름", 인자유형, flag, '단문자' }
    //      - 인자유형  : no_argument / required_argument / optional_argument
    //      - flag     : nullptr → switch-case에서 직접 처리
    //      - '단문자'  : optString 에도 반드시 등록
    //-----------------------------------------------------------
    static struct option longOptions[] = {
        {"enable",  required_argument, nullptr, 'e'},
        {"disable", required_argument, nullptr, 'd'},
        // 예시) {"scan",   no_argument,       nullptr, 's'},  // 새 옵션 추가 시
        {nullptr,   0,                nullptr,  0 }
    };

    //-----------------------------------------------------------
    // ② optString : 단문자 집합
    //    • 인자가 필요하면 뒤에 ':' 를 붙이기
    //    • longOptions[]와 반드시 일치할 것
    //-----------------------------------------------------------
    const char* optString = "e:d:";          // 예) "e:d:s:" ← -s <arg> 추가 시

    //-----------------------------------------------------------
    // getopt_long() 루프
    //    • optionChar : 단문자('e', 'd', …) 또는 0 / '?' / -1
    //    • optarg     : 인자가 있는 옵션의 값(char*)
    //-----------------------------------------------------------
    while ((optionChar = getopt_long(mArgc,
                                     mArgv,
                                     optString,
                                     longOptions,
                                     &optionIndex)) != -1)
    {
        //-------------------------------------------------------
        // ③ switch-case : 각 옵션별 동작 정의
        //    • 새 옵션을 추가했다면 여기에 case 문을 넣으세요.
        //-------------------------------------------------------
        switch (optionChar) {
        case 'e':                           // --enable <arg>
            mArgs.emplace_back("--enable");
            mArgs.emplace_back(optarg);
            break;

        case 'd':                           // --disable <arg>
            mArgs.emplace_back("--disable");
            mArgs.emplace_back(optarg);
            break;

        /* ---------- 새 옵션 예시 ----------
        case 's':                           // --scan <arg>
            mArgs.emplace_back("--scan");
            mArgs.emplace_back(optarg);
            break;
        -----------------------------------*/

        // getopt_long()이 '?'(알 수 없는 옵션)일 때는
        // default 로 떨어지므로 예외 처리로 통일
        default:
            throw std::invalid_argument(
                "Unknown or malformed command-line option");
        }
    }

    //-----------------------------------------------------------
    // 옵션 뒤에 남은 “위치 인자”들을 그대로 mArgs에 저장
    //-----------------------------------------------------------
    if (optind < mArgc) {
        for (int i = optind; i < mArgc; ++i) {
            mArgs.emplace_back(mArgv[i]);
        }
    } 
    else {
        for (int i = 0; i < mArgc; ++i) {
            mArgs.emplace_back(mArgv[i]);
        }
    }

    if (mArgs.empty()) {
        throw std::invalid_argument("No command arguments provided");
    }
}

void CommandHandler::run()
{
    if (mArgs.size() < 1) {
        throw std::invalid_argument("No command provided");
    }

    const std::string& command = mArgs[0];

    
    // 명령어 유형 결정
    // 새 명령어 추가 시 else if와 command를 이용해 알맞은 함수 호출
    // 데몬 관련 명령어
    if (command == "init") {
        
        // DB 테이블 생성(DBManager의 InitSchema() 호출)
        // 악성코드 팀에서는 만들어진 DB에 데이터 삽입 과정도 필요(해시)
        DBManager::GetInstance().InitSchema();
        
        // TODO
        // 세 개의 데몬 LogDaemon, RealtimeMonitorDaemon, ScheduledScanDaemon 호출
        // 이미 데몬 프로세스가 돌아가고 있을 경우 중복 방지 필요
        // 아래는 임시방편 코드(if ~ return)
        if (fork() == 0) {
            RealtimeMonitorDaemon().run(); 
            exit(0);
        }

        if (fork() == 0) {
            LogCollectorDaemon().run();
            exit(0);
        }
        
        if (fork() == 0) {
            ScheduledScanDaemon().run();
            exit(0);
        }

        std::cout << "[INIT] 백그라운드 데몬 실행\n";
        return;
    }
    else if (command == "boot_check") {
        // TODO
        // 부팅 시 데몬 프로세스 돌리기
        // ScheduledScanDaemon은 이전 상태 관계없이 계속 실행
        // LogDaemon, RealtimeMonitorDaemon은 이전에 enable 상태였는지 disable상태였는지에 따라 구분
        // 현재 Makefile에 부팅 시 운영체제가 boot_check를 실행하도록 설정함
        return;
    }
    else if (command == "--enable" && mArgs.size() >= 2 &&
             mArgs[1] == "realtime_monitor")
    {
        // TODO
        // 데몬 enable 설정
        // LogDaemon, RealtimeMonitorDaemon을 실행
        // 해당 정보는 부팅 시에도 계속 남아 있어야 함
        // 이미 데몬 프로세스가 돌아가고 있을 경우 중복 방지 필요
        return;
    }
    else if (command == "--disable" && mArgs.size() >= 2 &&
             mArgs[1] == "realtime_monitor")
    {
        // 데몬 disable 설정
        // LogDaemon, RealtimeMonitorDaemon 실행 해제 
        // 해당 정보는 부팅 시에도 계속 남아 있어야 함    
        return;
    }

    // 기능 관련 명령어
    else if (command == "malscan") {
        sig::MalScan();
    }
    else if (command == "restore") {
        if (mArgs.size() < 2) {
            throw std::invalid_argument("Missing filename for restore command");
        }
        sig::Restore(mArgs[1]);
    }
    else if (command == "integscan") {
        fim::IntScan();
    }
    else if (command == "baseline") {
        fim::BaselineGen();
    }
    else if (command == "check_baseline"){
        fim::PrintBaseline();
    }
    else if (command == "check_integscan"){
        fim::PrintIntegscan();
    }
    else if (command == "man") {
        //TODO
        //사용자에게 매뉴얼을 보여주는 함수
        //공통
    }
    else if (command == "malreport") {
        //TODO
        //사용자에게 데이터베이스에 저장된 악성코드 스캔 결과를 보여주는 함수
        //시그니처 팀 담당
    }
    else {
        throw std::invalid_argument("Unknown command: " + command);
    }
    return ;
}