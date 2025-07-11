#pragma once
#include <string>

namespace sig 
{
void MalScan(std::ostream& out);
void Restore(const std::string& filename, std::ostream& out);
void CmdListReports(std::ostream& out);
void CmdShowReport(int id, std::ostream& out);
} // namespace sig