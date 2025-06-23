#include "SigCommandHandler.h"
#include "MalwareScan.h"

#include <iostream>

namespace sig {

void MalScan() {
    std::cout << "[sig] Executing malware scan..." << std::endl;
    MalwareScan malscan;
    malscan.run();
}

void Restore(const std::string& filename) {
    std::cout << "[sig] Restoring file: " << filename << std::endl;
    // TODO
}

} // namespace sig