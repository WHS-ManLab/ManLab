// RegisterCommands.cpp
#include "RegisterCommands.h"
#include "SigCommandHandler.h"
#include "FimCommandHandler.h"
#include "CommandHelp.h"
#include <functional>
#include <spdlog/spdlog.h>

void RegisterCommands(CommandBus& bus) {
    static SigCommandHandler sig;
    static FimCommandHandler fim;

    bus.Register("malscan", [&](const auto&, auto& out) {
        spdlog::info("[malscan] 명령 실행됨");
        sig.MalScan(out);
    });
    spdlog::debug("명령어 등록 완료: malscan");

    bus.Register("restore", [&](const auto& tokens, auto& out) {
        if (tokens.size() < 2)
        {
            spdlog::info("[restore] 인자 누락: 파일명 필요");
            out << "[!] 복구할 파일명을 입력하세요.\n";
        }
        else
        {
            spdlog::info("[restore] 명령 실행됨 - 대상: {}", tokens[1]);
            sig.Restore(tokens[1], out);
        }
    });
    spdlog::debug("명령어 등록 완료: restore");

    bus.Register("check_malreport", [&](const auto&, auto& out) {
        spdlog::info("[check_malreport] 명령 실행됨");
        sig.CmdShowRecentReports(out);  // 전체 리포트 본문까지 출력하는 함수
    });
    spdlog::debug("명령어 등록 완료: malreport");

    bus.Register("integscan", [&](const auto&, auto& out) {
        spdlog::info("[integscan] 명령 실행됨");
        fim.IntScan(out);
    });
    spdlog::debug("명령어 등록 완료: integscan");

    bus.Register("baseline", [&](const auto&, auto& out) {
        spdlog::info("[baseline] 명령 실행됨");
        fim.BaselineGen(out);
    });
    spdlog::debug("명령어 등록 완료: baseline");

    bus.Register("check_baseline", [&](const auto&, auto& out) {
        spdlog::info("[check_baseline] 명령 실행됨");
        fim.PrintBaseline(out);
    });
    spdlog::debug("명령어 등록 완료: check_baseline");

    bus.Register("check_integscan", [&](const auto&, auto& out) {
        spdlog::info("[check_integscan] 명령 실행됨");
        fim.PrintIntegscan(out);
    });
    spdlog::debug("명령어 등록 완료: check_integscan");

    bus.Register("man", [&](const auto&, auto& out) {
        spdlog::debug("[man] 명령 실행됨");
        PrintCommandHelp(out);
    });
    spdlog::debug("명령어 등록 완료: man");
}