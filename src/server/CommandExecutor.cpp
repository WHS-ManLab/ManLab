#include "CommandExecutor.h"
#include "FimCommandHandler.h"
#include "SigCommandHandler.h"
#include "UserNotifier.h"
#include <iostream>


void CommandExecutor::Execute(const std::vector<std::string>& tokens, std::ostream& out)
{
    const std::string& cmd = tokens[0];

    if (cmd == "malscan")
    {
        sig::MalScan(out);
    }
    else if (cmd == "restore")
    {
        sig::Restore(tokens[1], out);
    }
    else if (cmd == "integscan")
    {
        fim::IntScan(out);
    }
    else if (cmd == "baseline")
    {
        fim::BaselineGen(out);
    }
    else if (cmd == "check_baseline")
    {
        fim::PrintBaseline(out);
    }
    else if (cmd == "check_integscan")
    {
        fim::PrintIntegscan(out);
    }
    else if (cmd == "man")
    {
        ShowManual(out);
    }
    else if (cmd == "malreport")
    {
        // 유효성 검사는 CommandHandler에서 완료되었음
        const std::string& subcmd = tokens[1];

        if (subcmd == "list")
        {
            sig::CmdListReports(out);
        }
        else if (subcmd == "view")
        {
            int id = std::stoi(tokens[2]);  // 변환 실패는 위에서 이미 처리됨
            sig::CmdShowReport(id, out);
        }
    }
}

void CommandExecutor::ShowManual(std::ostream& out)
{
    out << "매뉴얼\n"
        << "  ManLab run    : ManLab을 실행합니다.\n"
        << "  restore <file>  Restore quarantined file\n"
        << "  integscan       Run integrity scan\n"
        << "  baseline        Generate baseline\n"
        << "  check_baseline  Show baseline\n"
        << "  check_integscan Show integrity report\n"
        << "  man             Show this help\n";
}
