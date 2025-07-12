#pragma once

#include <string>
#include <set>
#include "DBManager.h"

class BaselineGenerator
{
public:
    BaselineGenerator(const std::string& iniPath, const std::string& dbPath);

    void GenerateAndStore();

    static std::string ComputeMd5(const std::string& filePath);
    static BaselineEntry CollectMetadata(const std::string& path, const std::string& md5);

private:
    std::string mIniPath;
    std::string mDbPath;

    void ParseIniAndStore();
    bool IsExcluded(const std::string& path, const std::set<std::string>& excludes);
};
