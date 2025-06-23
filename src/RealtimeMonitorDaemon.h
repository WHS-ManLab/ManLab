#pragma once
#include "DaemonBase.h"

class RealtimeMonitorDaemon : public DaemonBase {
public:
    void run() override;
};