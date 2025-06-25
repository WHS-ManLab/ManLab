#pragma once
#include "DaemonBase.h"

class LogCollectorDaemon : public DaemonBase {
public:
    void run() override;
};