#include "ScheduledScan.h"
#include "MalwareScan.h"

// 객체 생성
ScheduledScan::ScheduledScan() {}

void ScheduledScan::RunScan()
{
    MalwareScan scan;
    scan.Init();
    scan.SetMode(MalwareScan::Mode::Scheduled); 
    scan.Run(nullptr);
    scan.SendNotification();
    scan.SaveReportToDB();
}