#include "ConfigUtils.h"
#include "MessageService.hpp"

#include <algorithm>
#include <filesystem>

namespace ConfigUtils {

    YAML::Node loadYAML(const std::string& path) {
        if (!std::filesystem::exists(path)) {
            throw std::runtime_error("Config file not found: " + path);
        }
        try {
            return YAML::LoadFile(path);
        } catch (const YAML::Exception& e) {
            // e.what() includes the line and column of the problem
            throw std::runtime_error("Could not parse YAML config " + path + ": " + e.what());
        }
    }

    void warnUnknownKeys(const YAML::Node& node, const std::vector<std::string>& allowedKeys, const std::string& context) {
        if (!node.IsMap()) return;
        for (const auto& kv : node) {
            const auto key = kv.first.as<std::string>();
            if (std::find(allowedKeys.begin(), allowedKeys.end(), key) == allowedKeys.end()) {
                WARNING(context, ": unknown key '", key, "' (line ", kv.first.Mark().line + 1, ") will be ignored. Typo?");
            }
        }
    }

    // ── GRL config ──────────────────────────────────────────────────────────
    GRLConfig readGRLConfig(const std::string& configPath) {
        INFO("Reading GRL config from ", configPath, "...");
        const YAML::Node root = loadYAML(configPath);
        warnUnknownKeys(root, {"grl_jsons", "grl_csvs"}, configPath);

        GRLConfig config;
        config.grlJsons = getRequired<std::vector<std::string>>(root, "grl_jsons", configPath);
        config.grlCsvs  = getRequired<std::vector<std::string>>(root, "grl_csvs",  configPath);

        INFO("Read GRL config: ", config.grlJsons.size(), " JSON files, ", config.grlCsvs.size(), " CSV files.");
        return config;
    }

    // ── File config ─────────────────────────────────────────────────────────
    FileConfig readFileConfig(const std::string& configPath) {
        INFO("Reading file config from ", configPath, "...");
        const YAML::Node root = loadYAML(configPath);
        warnUnknownKeys(root, {"runs"}, configPath);

        const YAML::Node runs = root["runs"];
        if (!runs || !runs.IsMap()) {
            throw std::runtime_error(configPath + ": expected a 'runs:' map of run number -> {data_paths, waveform_paths}");
        }

        FileConfig fileConfig;
        for (const auto& kv : runs) {
            int runNumber;
            try {
                runNumber = kv.first.as<int>();
            } catch (const YAML::Exception&) {
                throw std::runtime_error(configPath + ": run key '" + kv.first.as<std::string>() + "' (line " +
                                         std::to_string(kv.first.Mark().line + 1) + ") is not an integer run number");
            }

            const std::string context = configPath + ": run " + std::to_string(runNumber);
            if (fileConfig.count(runNumber)) {
                throw std::runtime_error(context + " is defined more than once");
            }

            const YAML::Node& cfg = kv.second;
            if (!cfg.IsMap()) {
                throw std::runtime_error(context + ": expected a map with data_paths / waveform_paths");
            }
            warnUnknownKeys(cfg, {"data_paths", "waveform_paths"}, context);

            RunFiles files;
            files.dataPaths     = getRequired<std::vector<std::string>>(cfg, "data_paths", context);
            files.waveformPaths = getOptional<std::vector<std::string>>(cfg, "waveform_paths", {}, context);

            if (files.dataPaths.empty()) {
                throw std::runtime_error(context + ": data_paths is empty");
            }
            fileConfig[runNumber] = files;
        }

        INFO("Read file config: ", fileConfig.size(), " runs.");
        return fileConfig;
    }

} // namespace ConfigUtils
