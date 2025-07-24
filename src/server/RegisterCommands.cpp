// RegisterCommands.cpp
#include "RegisterCommands.h"
#include "SigCommandHandler.h"
#include "FimCommandHandler.h"
#include <functional>

void RegisterCommands(CommandBus& bus) {
    static SigCommandHandler sig;
    static FimCommandHandler fim;

    bus.Register("malscan", [&](const auto&, auto& out) {
        sig.MalScan(out);
    });

    bus.Register("restore", [&](const auto& tokens, auto& out) {
        if (tokens.size() < 2)
        {
            out << "[!] 복구할 파일명을 입력하세요.\n";
        }
        else
        {
            sig.Restore(tokens[1], out);
        }
    });

    bus.Register("malreport", [&](const auto& tokens, auto& out) {
        if (tokens.size() < 2) 
        {
            out << "[!] 서브 명령이 필요합니다 (list/view).\n";
            return;
        }

        if (tokens[1] == "list") 
        {
            sig.CmdListReports(out);
        }
        else if (tokens[1] == "view") 
        {
            if (tokens.size() < 3) out << "[!] 리포트 ID가 필요합니다.\n";
            else sig.CmdShowReport(std::stoi(tokens[2]), out);
        }
    });

    bus.Register("integscan", [&](const auto&, auto& out) {
        fim.IntScan(out);
    });

    bus.Register("baseline", [&](const auto&, auto& out) {
        fim.BaselineGen(out);
    });

    bus.Register("check_baseline", [&](const auto&, auto& out) {
        fim.PrintBaseline(out);
    });

    bus.Register("check_integscan", [&](const auto&, auto& out) {
        fim.PrintIntegscan(out);
    });

    bus.Register("man", [&](const auto&, auto& out) {
        out << "=== 지원 명령어 목록 ===\n"
            << "malscan\nrestore <file>\n"
            << "integscan\nbaseline\ncheck_baseline\ncheck_integscan\n"
            << "malreport list\nmalreport view <id>\nman\n";
    });
}