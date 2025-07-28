#pragma once
#include <ostream>
#include <string>

class SigCommandHandler 
{
public:
    void MalScan(std::ostream& out);
    void Restore(const std::string& filename, std::ostream& out);
    void CmdShowRecentReports(std::ostream& out);
};