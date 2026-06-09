#pragma once
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <iostream>
#include <nlohmann/json.hpp>
#include "TString.h"

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace GRLUtils {

    std::vector<TString> toTStringVector(const std::vector<std::string>& stdStrings);

    std::vector<std::string> toStdStringVector(const std::vector<TString>& tStrings);
    
    struct GRLConfig {
        std::vector<TString> grlJsons;
        std::vector<TString> grlCsvs;
    };

    GRLConfig readGRLConfig(const std::string& configPath);

    static std::vector<fs::path> collectFiles(const std::string& dir, const std::string& ext);


    static void stripTrailing(std::string& s, const std::string& suffix);
    
    std::unordered_map<int, float> getRunNumberLumiDict(const std::vector<TString>& csvFiles);
    
    std::string makeExcludedTimesCut(const std::vector<TString>& jsonFiles);

    std::string makeGoodTimesCut(const std::vector<TString>& jsonFiles);

} // namespace GRLUtils