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
#include "INIReader.h" 
#include "DaemonUtils.h"
#include "INIReader.h" 
#include "DaemonUtils.h"

namespace fs = std::filesystem;

CommandHandler::CommandHandler(int argc, char** argv)
    : mArgc(argc), mArgv(argv)
{
    parseOptions(); 
}


// ------------------------------------------------------------
// CommandHandler::parseOptions()
// CLI 옵션을 getopt_long()으로 파싱하여 mArgs 벡터에 저장합니다.
//
// [사용법 및 확장 규칙]
// • 새 옵션을 추가하려면 아래 3개만 수정하면 됩니다:
//   ① longOptions[] : "이름", 인자유형, nullptr, '단문자'
//   ② optString     : 단문자를 나열, 인자 필요 시 ':' 추가
//   ③ switch-case   : 각 옵션에 대한 처리 로직 추가
//
// 예시:
//   longOptions[] = { {"scan", required_argument, nullptr, 's'}, ... };
//   optString = "s:"
//   case 's': mArgs.emplace_back("--scan"); mArgs.emplace_back(optarg); break;
//
// 현재는 별도의 CLI 옵션 없이 위치 인자만 처리합니다.
// ------------------------------------------------------------
void CommandHandler::parseOptions()
{
    optind = 0;
    int  optionChar;
    int  optionIndex = 0;

    static struct option longOptions[] = {
        // 예시: {"scan", required_argument, nullptr, 's'},
        {nullptr, 0, nullptr, 0}
    };

    const char* optString = "";  // 예시: "s:"

    while ((optionChar = getopt_long(mArgc,
                                     mArgv,
                                     optString,
                                     longOptions,
                                     &optionIndex)) != -1)
    {
        switch (optionChar) {
        /* ---------- 옵션 처리 예시 ----------
        case 's':
            mArgs.emplace_back("--scan");
            mArgs.emplace_back(optarg);
            break;
        -------------------------------------*/
        default:
            // main.cpp의 catch문으로 전송
            throw std::invalid_argument("Unknown or malformed command-line option");
        }
    }

    // 옵션 뒤에 남은 위치 인자를 저장
    if (optind < mArgc) {
        for (int i = optind; i < mArgc; ++i) {
            mArgs.emplace_back(mArgv[i]);
        }
    } else {
        for (int i = 0; i < mArgc; ++i) {
            mArgs.emplace_back(mArgv[i]);
        }
    }

    if (mArgs.empty()) {
        // main.cpp의 catch문으로 전송
        throw std::invalid_argument("No command arguments provided");
    }
}

void CommandHandler::run()
{
    if (mArgs.size() < 1) {
        // main.cpp의 catch문으로 전송
        throw std::invalid_argument("No command provided");
    }

    const std::string& command = mArgs[0];

    
    // 명령어 유형 결정
    // [init] 시스템 초기화
    if (command == "init") { // 사용자가 Make이후 ManLab init 호출
        
        // DB 테이블 생성(DBManager의 InitSchema() 호출)
        DBManager::GetInstance().InitSchema();

        // 악성코드 해시 DB 초기화
        // 초기화 단계에서는 clone이후 Make를 실행시키므로 Makefile이 있는 디렉토리 기준 상대경로 지정
        DBManager::InitHashDB("../malhash/malware_hashes.txt");
        
        // RealtimeMonitorDaemon 실행
        // 데몬 중복 살행 방지를 위한 실행 보조 함수
        // PID 파일 이름과 함수 포인터를 인자로 전달
        launchDaemonIfNotRunning("RealtimeMonitorDaemon", [](){RealtimeMonitorDaemon().run();});
        return;
    }

    // [reload] 설정 재적용 및 PC재부팅
    else if (command == "reload") {
        INIReader reader("/ManLab/conf/realtimeControl.ini");
        bool monitorEnabled = reader.GetBoolean("RealtimeControl", "realtimeMonitorEnabled", false);

        // RealtimeMonitorDaemon : PC 재부팅 시 항상 실행
        // 설정 재적용 시에는 중복 실행 방지 적용
        launchDaemonIfNotRunning("RealtimeMonitorDaemon", [](){RealtimeMonitorDaemon().run();});

        // PC 재부팅 시 설정 파일을 읽고 실행
        // 설정 재적용 시에도 동일한 동작
        if (monitorEnabled) {
            launchDaemonIfNotRunning("LogCollectorDaemon", []() {LogCollectorDaemon().run();});
            launchDaemonIfNotRunning("ScheduledScanDaemon", []() {ScheduledScanDaemon().run();});
        } else {
            stopDaemon("LogCollectorDaemon");
            stopDaemon("ScheduledScanDaemon");
        }

        return;
    }

    // [malscan] 악성코드 수동검사 명령어
    else if (command == "malscan") {
        sig::MalScan();
    }

    // [restore] 격리된 파일 복구 명령어
    else if (command == "restore") {
        if (mArgs.size() < 2) {
            // main.cpp의 catch문으로 전송 
            throw std::invalid_argument("Missing filename for restore command");
        }
        sig::Restore(mArgs[1]);
    }

    // [integscan] 무결성 수동검사 명령어
    else if (command == "integscan") {
        fim::IntScan();
    }

    // [baseline] 
    else if (command == "baseline") {
        fim::BaselineGen();
    }

    // [check_baseline]
    else if (command == "check_baseline"){
        fim::PrintBaseline();
    }

    // [man] 매뉴얼 디스플레이 명령어
    else if (command == "man") {
        //TODO
        //사용자에게 매뉴얼을 보여주는 함수
        //공통
    }

    // [malreport] 지금까지의 검사 내역 보여주는 명령어
    else if (command == "malreport") {
        //TODO
        //사용자에게 데이터베이스에 저장된 악성코드 스캔 결과를 보여주는 함수
        //시그니처 팀 담당
    }

    // 명령어 집합에 존재하지 않는 경우
    else {
        // main.cpp의 catch문으로 전송
        throw std::invalid_argument("Unknown command: " + command);
    }
    return ;
}