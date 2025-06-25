#pragma once
#include "DaemonBase.h"

class ScheduledScanDaemon : public DaemonBase {
public:
    void run() override;
};