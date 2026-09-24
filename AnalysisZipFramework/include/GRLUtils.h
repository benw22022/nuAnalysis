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
#include <map>

namespace fs = std::filesystem;
using json = nlohmann::json;

// Reading of the official FASER GRL files (JSON good-run lists and CSV luminosities).
// The framework's own config files are YAML and are read by ConfigUtils.
namespace GRLUtils {

    std::vector<TString> toTStringVector(const std::vector<std::string>& stdStrings);

    std::vector<std::string> toStdStringVector(const std::vector<TString>& tStrings);
    
    static std::vector<fs::path> collectFiles(const std::string& dir, const std::string& ext);


    static void stripTrailing(std::string& s, const std::string& suffix);
    
    std::unordered_map<int, float> getRunNumberLumiDict(const std::vector<TString>& csvFiles);
    
    std::string makeExcludedTimesCut(const std::vector<TString>& jsonFiles);

    std::string makeGoodTimesCut(const std::vector<TString>& jsonFiles);

} // namespace GRLUtils