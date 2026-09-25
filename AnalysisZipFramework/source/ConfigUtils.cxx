#include "ConfigUtils.h"
#include "MessageService.hpp"

#include <algorithm>
#include <filesystem>
#include <set>
#include <fnmatch.h>

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

    // ── Output columns config ───────────────────────────────────────────────
    OutputColumnsConfig readOutputColumnsConfig(const std::string& configPath) {
        INFO("Reading output columns config from ", configPath, "...");
        const YAML::Node root = loadYAML(configPath);
        warnUnknownKeys(root, {"nt"}, configPath);

        const YAML::Node nt = root["nt"];
        if (!nt || !nt.IsMap()) {
            throw std::runtime_error(configPath + ": expected an 'nt:' map (settings for the output nt tree)");
        }
        const std::string context = configPath + ": nt";
        warnUnknownKeys(nt, {"save_all_input_columns", "save_all_defined_columns", "keep", "keep_mc", "keep_data", "drop"}, context);

        OutputColumnsConfig config;
        config.saveAllInputColumns   = getOptional<bool>(nt, "save_all_input_columns",   false, context);
        config.saveAllDefinedColumns = getOptional<bool>(nt, "save_all_defined_columns", false, context);
        config.keep     = getOptional<std::vector<std::string>>(nt, "keep",      {}, context);
        config.keepMC   = getOptional<std::vector<std::string>>(nt, "keep_mc",   {}, context);
        config.keepData = getOptional<std::vector<std::string>>(nt, "keep_data", {}, context);
        config.drop     = getOptional<std::vector<std::string>>(nt, "drop",      {}, context);
        config.sourcePath = configPath;

        INFO("Output columns config: save_all_input_columns=", config.saveAllInputColumns,
             ", save_all_defined_columns=", config.saveAllDefinedColumns,
             ", keep: ", config.keep.size(), " (+", config.keepMC.size(), " MC, +", config.keepData.size(), " data)",
             ", drop: ", config.drop.size());
        return config;
    }

    const std::vector<std::string>& mandatoryOutputColumns() {
        static const std::vector<std::string> cols{"run", "eventID"};
        return cols;
    }

    namespace {
        bool isGlob(const std::string& s) { return s.find_first_of("*?[") != std::string::npos; }

        // All columns matching an exact name or a glob pattern
        std::vector<std::string> match(const std::string& entry, const std::vector<std::string>& columns) {
            std::vector<std::string> out;
            if (!isGlob(entry)) {
                if (std::find(columns.begin(), columns.end(), entry) != columns.end()) out.push_back(entry);
                return out;
            }
            for (const auto& c : columns) {
                if (fnmatch(entry.c_str(), c.c_str(), 0) == 0) out.push_back(c);
            }
            return out;
        }

        // Add the columns matching each entry of a keep list to `selected` (warn for entries matching nothing)
        void applyKeepList(const std::vector<std::string>& entries, const std::string& listName,
                           const std::vector<std::string>& allColumns, std::set<std::string>& selected,
                           const std::string& src) {
            for (const auto& entry : entries) {
                const auto matches = match(entry, allColumns);
                if (matches.empty()) {
                    WARNING(src, ": ", listName, " entry '", entry, "' ",
                            (isGlob(entry) ? "matches no column" : "does not exist"), " in the dataframe. It will not be saved.");
                }
                selected.insert(matches.begin(), matches.end());
            }
        }
    }

    std::vector<std::string> selectOutputColumns(const std::vector<std::string>& allColumns,
                                                 const std::vector<std::string>& definedColumns,
                                                 const OutputColumnsConfig& config,
                                                 bool isMC,
                                                 const std::vector<std::string>& notInSaveAll) {
        const std::string& src = config.sourcePath;
        const std::set<std::string> defined(definedColumns.begin(), definedColumns.end());
        const std::set<std::string> excluded(notInSaveAll.begin(), notInSaveAll.end());
        std::set<std::string> selected;

        for (const auto& c : allColumns) {
            if (excluded.count(c)) continue;
            const bool isDefined = defined.count(c) > 0;
            if (( isDefined && config.saveAllDefinedColumns) ||
                (!isDefined && config.saveAllInputColumns)) {
                selected.insert(c);
            }
        }

        // keep lists: the common one plus the one for this sample type
        applyKeepList(config.keep, "keep", allColumns, selected, src);
        if (isMC) applyKeepList(config.keepMC,   "keep_mc",   allColumns, selected, src);
        else      applyKeepList(config.keepData, "keep_data", allColumns, selected, src);

        // drop list (applied last)
        for (const auto& entry : config.drop) {
            const auto matches = match(entry, allColumns);
            if (matches.empty()) {
                WARNING(src, ": drop entry '", entry, "' ",
                        (isGlob(entry) ? "matches no column" : "does not exist"), " in the dataframe.");
            }
            for (const auto& c : matches) selected.erase(c);
        }

        // mandatory columns
        for (const auto& c : mandatoryOutputColumns()) {
            if (std::find(allColumns.begin(), allColumns.end(), c) == allColumns.end()) {
                WARNING("Column '", c, "' is not in the dataframe, so nt events cannot be matched to eventID_pass.");
                continue;
            }
            if (!selected.count(c)) {
                const bool dropped = std::any_of(config.drop.begin(), config.drop.end(),
                                                 [&](const std::string& d) { return !match(d, {c}).empty(); });
                if (dropped) WARNING(src, ": '", c, "' is always saved (needed to match events to eventID_pass); ignoring the drop entry.");
                selected.insert(c);
            }
        }

        std::vector<std::string> result;
        for (const auto& c : allColumns) {
            if (selected.count(c)) result.push_back(c);
        }
        return result;
    }

} // namespace ConfigUtils
