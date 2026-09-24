#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// ConfigUtils: reading the framework's YAML configuration files (yaml-cpp).
//
// All of the framework's own configs are YAML so that they can carry comments.
// The official FASER GRL files (cvmfs) are JSON and are still read by GRLUtils.
// ─────────────────────────────────────────────────────────────────────────────
#include <map>
#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>

namespace ConfigUtils {

    // ── Generic helpers (reusable for future cut / histogram configs) ────────

    // Load a YAML file. Throws std::runtime_error (with file name, line and column) if the
    // file does not exist or cannot be parsed.
    YAML::Node loadYAML(const std::string& path);

    // Return node[key] converted to T. Throws with a clear message if the key is missing
    // or has the wrong type. `context` is used in the error message (e.g. "file_config.yaml: run 7729").
    template <typename T>
    T getRequired(const YAML::Node& node, const std::string& key, const std::string& context);

    // As getRequired, but returns `fallback` if the key is missing or null (e.g. "waveform_paths:" with no value)
    template <typename T>
    T getOptional(const YAML::Node& node, const std::string& key, const T& fallback, const std::string& context);

    // Print a warning for every key in `node` that is not in `allowedKeys` (catches typos)
    void warnUnknownKeys(const YAML::Node& node, const std::vector<std::string>& allowedKeys, const std::string& context);

    // ── GRL config (config/grl_config.yaml) ─────────────────────────────────
    struct GRLConfig {
        std::vector<std::string> grlJsons;  // official FASER GRLs: stable-beam / excluded periods per run
        std::vector<std::string> grlCsvs;   // official FASER GRLs: recorded luminosity per run
    };

    GRLConfig readGRLConfig(const std::string& configPath);

    // ── File config (config/file_config.yaml) ───────────────────────────────
    struct RunFiles {
        std::vector<std::string> dataPaths;      // physics NTuples (wildcards allowed)
        std::vector<std::string> waveformPaths;  // aux NTuples with the VetoNu reduced charge (may be empty)
    };

    using FileConfig = std::map<int, RunFiles>;  // run number -> files

    FileConfig readFileConfig(const std::string& configPath);

} // namespace ConfigUtils

// ── Template implementations ────────────────────────────────────────────────
#include <stdexcept>

namespace ConfigUtils {

    template <typename T>
    T getRequired(const YAML::Node& node, const std::string& key, const std::string& context) {
        const YAML::Node value = node[key];
        if (!value || value.IsNull()) {
            throw std::runtime_error(context + ": missing required key '" + key + "'");
        }
        try {
            return value.as<T>();
        } catch (const YAML::Exception& e) {
            throw std::runtime_error(context + ": key '" + key + "' has the wrong type (line " +
                                     std::to_string(value.Mark().line + 1) + ")");
        }
    }

    template <typename T>
    T getOptional(const YAML::Node& node, const std::string& key, const T& fallback, const std::string& context) {
        const YAML::Node value = node[key];
        if (!value || value.IsNull()) return fallback;
        return getRequired<T>(node, key, context);
    }

} // namespace ConfigUtils
