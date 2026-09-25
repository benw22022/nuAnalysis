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
    
    
    std::unordered_map<int, float> getRunNumberLumiDict(const std::vector<TString>& csvFiles);
    
    // Time ranges [start, stop] (unix time, inclusive) per run, for fast lookup in the event loop
    class TimeRanges {
    public:
        void add(int run, long long start, long long stop) { m_ranges[run].emplace_back(start, stop); ++m_n; }
        // True if eventTime lies inside one of the ranges for this run (false if the run has none)
        bool contains(int run, long long eventTime) const {
            auto it = m_ranges.find(run);
            if (it == m_ranges.end()) return false;
            for (const auto& [start, stop] : it->second) {
                if (eventTime >= start && eventTime <= stop) return true;
            }
            return false;
        }
        std::size_t size() const { return m_n; }   // total number of ranges
        bool hasRun(int run) const { return m_ranges.count(run) > 0; }
    private:
        std::unordered_map<int, std::vector<std::pair<long long, long long>>> m_ranges;
        std::size_t m_n{0};
    };

    struct GRLTimes {
        TimeRanges stable;    // "stable_list":   good (stable beam) periods
        TimeRanges excluded;  // "excluded_list": periods to remove
    };

    // Parse the official FASER GRL .json files (run -> stable_list / excluded_list)
    GRLTimes readGRLTimes(const std::vector<TString>& jsonFiles);

} // namespace GRLUtils