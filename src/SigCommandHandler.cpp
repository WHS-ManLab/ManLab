#include "SigCommandHandler.h"
#include "MalwareScan.h"
#include "RestoreManager.h"

#include <iostream>
#include <chrono>

namespace sig {

void MalScan()
{
    using namespace std;
    using namespace std::chrono;

    cout << "Executing malware scan..." << endl;

    system_clock::time_point start = system_clock::now();
    MalwareScan malscan;
    malscan.Run();
    system_clock::time_point end = system_clock::now();

    malscan.PrintReport(start, end);
}

void Restore(const std::string& filename)
{
    using namespace std;
    cout << "Restoring file: " << filename << endl;

    RestoreManager restorer(filename);
    restorer.Run();

    if (restorer.IsSuccess())
    {
        cout << "[+] 복구 성공: " << filename << endl;
    }
    else
    {
        cerr << "[-] 복구 실패: " << filename << endl;
    }
}

} // namespace sig
