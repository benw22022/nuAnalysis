#include "GRLUtils.h"
#include "MessageService.hpp"

namespace GRLUtils {

    std::vector<TString> toTStringVector(const std::vector<std::string>& stdStrings) {
        return std::vector<TString>(stdStrings.begin(), stdStrings.end());
    }

    std::vector<std::string> toStdStringVector(const std::vector<TString>& tStrings) {
        std::vector<std::string> result;
        result.reserve(tStrings.size());
        for (const auto& s : tStrings)
            result.emplace_back(s.Data());
        return result;
    }


    // ─── Functions ───────────────────────────────────────────────────────────────

    /**
    * Parse CSV files in pathToGrls to build a map of run number → recorded luminosity.
    * Expects run number in column 0 and lumi_rec in column 3 (0-indexed), with a header row.
    */
    std::unordered_map<int, float> getRunNumberLumiDict(const std::vector<TString>& csvFiles) {
        
        if (csvFiles.empty()) {
            ERROR("No GRL .csv found!");
            throw std::runtime_error("No files found");
        }

        std::unordered_map<int, float> runLumiDict;

        for (const auto& fpath : csvFiles) {
            std::ifstream f(fpath);
            if (!f.is_open()) {
                ERROR("Could not open GRL CSV file: ", fpath);
                throw std::runtime_error("File open error");
            }
            
            INFO("Parsing GRL CSV file ", fpath, " for run luminosities...");

            std::string line;
            int lineNum = 0;

            while (std::getline(f, line)) {
                if (lineNum++ == 0) continue;       // skip header
                if (line.empty() || line[0] == '#') continue;

                std::stringstream ss(line);
                std::string token;
                std::vector<std::string> tokens;
                while (std::getline(ss, token, ','))
                    tokens.push_back(token);

                if (tokens.size() < 4) continue;
                int   runNumber = std::stoi(tokens[0]);
                float lumiRec   = std::stof(tokens[3]);
                runLumiDict[runNumber] = lumiRec;
            }
        }

        return runLumiDict;
    }

    /**
    * Parse the official FASER GRL JSON files into per-run time ranges:
    *   stable_list   -> good (stable beam) periods, used for the "Good times" cut
    *   excluded_list -> periods to remove, used for the "Excluded times" cut
    * Ranges are inclusive: start_utime <= eventTime <= stop_utime.
    * (Replaces the previous approach of building one huge JIT filter string.)
    */
    GRLTimes readGRLTimes(const std::vector<TString>& jsonFiles) {

        if (jsonFiles.empty()) {
            ERROR("No GRL .json found!");
            throw std::runtime_error("No files found");
        }

        GRLTimes times;
        for (const auto& grlFile : jsonFiles) {
            std::ifstream f(grlFile);
            if (!f.is_open()) {
                ERROR("Could not open GRL JSON file: ", grlFile);
                throw std::runtime_error("File open error");
            }

            INFO("Parsing GRL JSON file ", grlFile, " for good and excluded times...");
            const json grlDict = json::parse(f);

            for (const auto& [runStr, runInfo] : grlDict.items()) {
                const int run = std::stoi(runStr);
                if (runInfo.contains("stable_list")) {
                    for (const auto& r : runInfo.at("stable_list")) {
                        times.stable.add(run, r.at("start_utime").get<long long>(), r.at("stop_utime").get<long long>());
                    }
                }
                if (runInfo.contains("excluded_list")) {
                    for (const auto& r : runInfo.at("excluded_list")) {
                        times.excluded.add(run, r.at("start_utime").get<long long>(), r.at("stop_utime").get<long long>());
                    }
                }
            }
        }

        INFO("Read ", times.stable.size(), " good (stable) periods and ", times.excluded.size(), " excluded periods from the GRL.");
        return times;
    }

}