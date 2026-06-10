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

    GRLConfig readGRLConfig(const std::string& configPath) {
        std::ifstream ifs(configPath);
        if (!ifs.is_open())
        {
            ERROR("Could not open config file: ", configPath);
            throw std::runtime_error("Could not open config file: " + configPath);
        }

        INFO("Reading GRL config from ", configPath, "...");
        
        json j = json::parse(ifs);

        GRLConfig config;
        config.grlJsons = toTStringVector(j.at("GRLJsons").get<std::vector<std::string>>());
        config.grlCsvs  = toTStringVector(j.at("GRLCSVs").get<std::vector<std::string>>());
        
        ifs.close();
        INFO("Read GRL config: ", config.grlJsons.size(), " JSON files, ", config.grlCsvs.size(), " CSV files.");
        return config;
    }   

    // ─── Helper ──────────────────────────────────────────────────────────────────

    // Collect all files with a given extension from a directory
    static std::vector<fs::path> collectFiles(const std::string& dir, const std::string& ext) {
        std::vector<fs::path> files;
        for (const auto& entry : fs::directory_iterator(dir))
            if (entry.path().extension() == ext)
                files.push_back(entry.path());
        return files;
    }

    // Strip a trailing suffix from a string (mirrors Python's rstrip on a token)
    static void stripTrailing(std::string& s, const std::string& suffix) {
        if (s.size() >= suffix.size() &&
            s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0)
            s.erase(s.size() - suffix.size());
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
    * Parse JSON GRL files to build a filter string that removes excluded time periods.
    * Returns a string suitable for use with RDataFrame::Filter().
    * Returns "" if there are no excluded periods.
    */
    std::string makeExcludedTimesCut(const std::vector<TString>& jsonFiles) {

        if (jsonFiles.empty()) {
            ERROR("No GRL .json found!");
            throw std::runtime_error("No files found");
        }
        
        struct TimeRange { long long start; long long stop; };
        std::unordered_map<std::string, std::vector<TimeRange>> excludedTimes;
        int nExcluded = 0;

        for (const auto& grlFile : jsonFiles) {
            INFO("Parsing GRL JSON file ", grlFile, " for excluded times...");
            std::ifstream f(grlFile);
            if (!f.is_open()) {
                ERROR("Could not open GRL JSON file: ", grlFile);
                throw std::runtime_error("File open error");
            }
            json grlDict = json::parse(f);

            for (const auto& [runNumber, runInfo] : grlDict.items()) {
                if (!runInfo.contains("excluded_list")) continue;
                for (const auto& excl : runInfo["excluded_list"]) {
                    excludedTimes[runNumber].push_back({
                        excl["start_utime"].get<long long>(),
                        excl["stop_utime"].get<long long>()
                    });
                    ++nExcluded;
                }
            }
        }

        if (nExcluded == 0) return "";

        INFO("Applying cuts to remove ", nExcluded, " excluded periods...");

        std::string cutStr;
        for (const auto& [runNumber, exclusionList] : excludedTimes) {
            for (const auto& range : exclusionList) {
                cutStr += "((eventTime >= " + std::to_string(range.start) +
                        ") && (eventTime <= " + std::to_string(range.stop) +
                        ") && (run == " + runNumber + "))";
                if (nExcluded > 1) cutStr += " || ";
            }
        }

        stripTrailing(cutStr, " || ");
        return cutStr;
    }

    /**
    * Parse JSON GRL files to build a filter string that selects stable (good) time periods.
    * Returns a string suitable for use with RDataFrame::Filter().
    */
    std::string makeGoodTimesCut(const std::vector<TString>& jsonFiles) {

        if (jsonFiles.empty()) {
            ERROR("No GRL .json found!");
            throw std::runtime_error("No files found");
        }

        struct TimeRange { long long start; long long stop; };
        std::unordered_map<std::string, std::vector<TimeRange>> goodTimes;
        int nGood = 0;

        for (const auto& grlFile : jsonFiles) {
            std::ifstream f(grlFile);
            
            if (!f.is_open()) {
                ERROR("Could not open GRL JSON file: ", grlFile);
                throw std::runtime_error("File open error");
            }

            INFO("Parsing GRL JSON file ", grlFile, " for good times...");
            json grlDict = json::parse(f);

            for (const auto& [runNumber, runInfo] : grlDict.items()) {
                for (const auto& stable : runInfo["stable_list"]) {
                    goodTimes[runNumber].push_back({
                        stable["start_utime"].get<long long>(),
                        stable["stop_utime"].get<long long>()
                    });
                    ++nGood;
                }
            }
        }

        std::string cutStr;
        for (const auto& [runNumber, stableList] : goodTimes) {
            for (const auto& range : stableList) {
                cutStr += "((eventTime >= " + std::to_string(range.start) +
                        ") && (eventTime <= " + std::to_string(range.stop) +
                        ") && (run == " + runNumber + "))";
                if (nGood > 1) cutStr += " || ";
            }
        }

        stripTrailing(cutStr, " || ");
        return cutStr;
    }

    FileConfig parseFileConfig(std::string configPath) {
        std::ifstream ifs(configPath);
        if (!ifs.is_open()) {
            ERROR("Could not open file config: ", configPath);
            throw std::runtime_error("Could not open file config: " + configPath);
        }

        INFO("Reading file config from ", configPath, "...");
        
        json j = json::parse(ifs);

        FileConfig fileConfig;

        for (const auto& [runStr, paths] : j.items()) {
            int runNumber = std::stoi(runStr);
            std::vector<std::string> dataPaths = paths["data_paths"].get<std::vector<std::string>>();
            std::vector<std::string> auxPaths  = paths["waveform_paths"].get<std::vector<std::string>>();
            fileConfig[runNumber] = {dataPaths, auxPaths};
        }

        return fileConfig;
    }

}