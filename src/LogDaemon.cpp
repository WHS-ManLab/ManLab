#include "LogDaemon.h"
#include "RsyslogManager.h"
#include "AuditLogManager.h"

#include <thread>
#include <chrono>
std::atomic<bool> LogCollectorDaemon::running(true);

void LogCollectorDaemon::run()
{
    daemonize();
    setupSignalHandlers();

    std::string logPath = "/var/log/manlab.log";
    std::string ruleSetPath = "Manlab/conf/RsyslogRuleSet.yaml";

    RsyslogManager rsyslog(logPath, ruleSetPath);
    AuditLogManager auditlog;

    std::thread rsyslogThread([&rsyslog]()
                              { rsyslog.RsyslogRun(); });

    std::thread auditlogThread([&auditlog]()
                               { auditlog.Run(); });

    while (running)
    {
        // TODO: 로그 파일 감시 및 분석
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    if (rsyslogThread.joinable())
    {
        rsyslogThread.join();
    }
    if (auditlogThread.joinable())
    {
        auditlogThread.join();
    }
}