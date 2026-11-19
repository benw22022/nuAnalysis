#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// ConfigUtils: reading the framework's YAML configuration files (yaml-cpp).
//
// All of the framework's own configs are YAML so that they can carry comments.
// The official FASER GRL files (cvmfs) are JSON and are still read by GRLUtils.
// ─────────────────────────────────────────────────────────────────────────────
#include <map>
#include <optional>
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

    // ── Config parts ─────────────────────────────────────────────────────────
    // Each config type can be put together from several parts, concatenated in order: config files
    // and inline entries of an analysis config (see readAnalysisConfig). Names (cuts, definitions,
    // categories, histograms, runs) must be unique across all parts of a config.
    struct ConfigPart {
        YAML::Node node;     // file: the whole file; inline: the entries, as under the section key of that file type
        std::string origin;  // file path, or "<analysis config>: <section> entry <n> (inline, line <l>)"
        bool isFile{true};
        std::string text;    // the file as read, or the inline entry as YAML (stored in the output file)
    };

    // Load a config file as a part (throws if it does not exist or cannot be parsed)
    ConfigPart configFilePart(const std::string& path);

    // "a.yaml", "a.yaml + b.yaml + <inline entry>", or "(none)": for log messages
    std::string describeParts(const std::vector<ConfigPart>& parts);

    // ── GRL config (config/grl_config.yaml) ─────────────────────────────────
    struct GRLConfig {
        std::vector<std::string> grlJsons;  // official FASER GRLs: stable-beam / excluded periods per run
        std::vector<std::string> grlCsvs;   // official FASER GRLs: recorded luminosity per run
    };

    GRLConfig readGRLConfig(const std::string& configPath);
    GRLConfig readGRLConfig(const std::vector<ConfigPart>& parts);      // lists of all parts concatenated

    // ── File config (config/file_config.yaml) ───────────────────────────────
    struct RunFiles {
        std::vector<std::string> dataPaths;      // physics NTuples (wildcards allowed)
        std::vector<std::string> waveformPaths;  // aux NTuples with the VetoNu reduced charge (may be empty)
        // Optional information about the sample (written to the meta tree)
        std::string description;                 // e.g. "2024 CaloNu EPOS pi"
        std::optional<bool> isNominal;           // nominal MC sample (true) or a variation (false)
        std::optional<double> lumiSample;        // generated luminosity of an MC sample, fb^-1
    };

    using FileConfig = std::map<int, RunFiles>;  // run number -> files

    FileConfig readFileConfig(const std::string& configPath);
    FileConfig readFileConfig(const std::vector<ConfigPart>& parts);    // runs of all parts (unique run numbers)

    // ── Output columns config (config/output_columns.yaml) ──────────────────
    // Which columns of the dataframe are written to the output 'nt' tree.
    // Entries in keep / keep_mc / keep_data / drop are exact column names or glob patterns
    // (*, ?, [...]), e.g. "LeadTrack_*".
    struct OutputColumnsConfig {
        bool saveAllInputColumns{false};    // every branch of the input NTuple (incl. aliases)
        bool saveAllDefinedColumns{false};  // every column created with Define/Redefine at runtime
        std::vector<std::string> keep;      // extra columns for all samples
        std::vector<std::string> keepMC;    // extra columns for MC only (e.g. truth)
        std::vector<std::string> keepData;  // extra columns for data only
        std::vector<std::string> drop;      // removed from the selection (applied last)
        std::string sourcePath;             // config file(s) this was read from (for log messages)
    };

    OutputColumnsConfig readOutputColumnsConfig(const std::string& configPath);
    // save_all_* switches: true if true in any part; keep / drop lists: concatenated
    OutputColumnsConfig readOutputColumnsConfig(const std::vector<ConfigPart>& parts);

    // ── Selection config (config/cuts.yaml): cuts and histograms ─────────────
    // One histogram axis: a column and EITHER a fixed binning (bins, min, max) OR bin edges
    struct AxisConfig {
        std::string variable;
        int nBins{0};
        double min{0.}, max{0.};
        std::vector<double> edges;   // variable binning if not empty
        bool hasEdges() const { return !edges.empty(); }
    };

    struct HistogramConfig {
        std::string name;                   // unique in the whole config (used as the object name in the output file)
        std::string title;                  // defaults to name
        std::string dataType{"ALL"};        // ALL, DATA, MC or an MC category data type (see cuts.yaml)
        std::vector<std::string> requirements;  // e.g. reduced_charge (see knownRequirements())
        AxisConfig x;
        std::optional<AxisConfig> y;        // 2D if set
        bool split{true};                   // on MC: also book one copy per MC category (categories.yaml)
    };

    struct CutConfig {
        std::string name;                   // cutflow name (also used for the eventID_pass flag)
        std::string expression;             // RDataFrame filter expression
        std::string dataType{"ALL"};
        std::vector<std::string> requirements;
        std::vector<HistogramConfig> histograms;  // booked after this cut
        std::vector<HistogramConfig> nMinus1;     // N-1 histograms: all other applied cuts, not this one
    };

    struct SelectionConfig {
        std::vector<HistogramConfig> histograms;  // top-level 'Histograms': booked before the first cut
        std::vector<CutConfig> cuts;              // in file order
        std::string sourcePath;
    };

    // categoryDataTypes: data_type values allowed in addition to ALL/DATA/MC
    // (from categoryDataTypes(CategoriesConfig); empty if no MC categories are defined)
    SelectionConfig readSelectionConfig(const std::string& configPath,
                                        const std::vector<std::string>& categoryDataTypes = {});
    // Cuts and Histograms of all parts concatenated (inline entries: cuts only, as under 'Cuts:')
    SelectionConfig readSelectionConfig(const std::vector<ConfigPart>& parts,
                                        const std::vector<std::string>& categoryDataTypes = {});

    // ── MC categories config (config/categories.yaml) ───────────────────────
    // Event-level MC categories (e.g. signal / background components). On MC, histograms are
    // also booked once per category, and a cutflow is written per category.
    struct CategoryConfig {
        std::string name;        // valid C++ identifier: flag column category_<name>, output directory name
        std::string expression;  // RDataFrame expression, true if the event is in this category
        std::string type;        // "signal" or "background"
        std::string latex;       // label for plots (defaults to the name)
        std::string colour;      // colour for plots (free text, may be empty)
        std::string origin;      // config file or inline entry it comes from
    };

    struct CategoriesConfig {
        std::vector<CategoryConfig> categories;  // in file order
        std::string sourcePath;
    };

    CategoriesConfig readCategoriesConfig(const std::string& configPath);
    CategoriesConfig readCategoriesConfig(const std::vector<ConfigPart>& parts);

    // data_type values that select MC categories on cuts and histograms: "signal", "background",
    // "other" and the category names (empty if there are no categories)
    std::vector<std::string> categoryDataTypes(const CategoriesConfig& config);

    // ── Definitions config (config/definitions.yaml): new columns ───────────
    struct DefinitionConfig {
        std::string name;                   // new column name (must be a valid C++ identifier)
        std::string expression;             // RDataFrame Define expression
        std::string dataType{"ALL"};        // same meaning as for cuts
        std::vector<std::string> requirements;
        std::string origin;                 // config file or inline entry it comes from
    };

    struct DefinitionsConfig {
        std::vector<DefinitionConfig> definitions;  // in file order
        std::string sourcePath;
    };

    DefinitionsConfig readDefinitionsConfig(const std::string& configPath);
    DefinitionsConfig readDefinitionsConfig(const std::vector<ConfigPart>& parts);

    // ── Analysis config: which configs an analysis uses ─────────────────────
    // A YAML file with up to seven sections (keys are case-insensitive):
    //   file_configs, cuts, definitions, categories, output, eventid_pass, grl
    // Each section is a list of config files (paths relative to the analysis config's directory, or
    // absolute) and/or inline entries, written as under the section key of that file type. A section
    // that is left out means no config of that type.
    struct AnalysisConfig {
        std::string path;
        std::string text;                                     // the file as read
        std::map<std::string, std::vector<ConfigPart>> sections;  // only the sections that are given
        // Parts of a section, or nullopt if the analysis config does not have that section
        std::optional<std::vector<ConfigPart>> section(const std::string& name) const;
    };

    // ── Extra eventID_pass columns (analysis config section EventID_pass) ───
    // Columns written to the eventID_pass tree in addition to run, eventID, the passed_<cut> flags and
    // mc_category. Entries are exact column names or glob patterns (as in the output columns config).
    // File form: eventID_pass: {keep: [...], keep_mc: [...], keep_data: [...]}; inline entry: the map itself.
    struct EventIDColumnsConfig {
        std::vector<std::string> keep;      // for all samples
        std::vector<std::string> keepMC;    // MC only
        std::vector<std::string> keepData;  // data only
        std::string sourcePath;
    };

    EventIDColumnsConfig readEventIDColumnsConfig(const std::vector<ConfigPart>& parts);

    // The extra columns for this sample, in the order of allColumns. WARNING for entries matching no column.
    std::vector<std::string> selectEventIDColumns(const std::vector<std::string>& allColumns,
                                                  const EventIDColumnsConfig& config, bool isMC);

    // Section names, in the order they are listed in log messages
    const std::vector<std::string>& analysisConfigSections();

    AnalysisConfig readAnalysisConfig(const std::string& configPath);

    // Allowed values for data_type and requires
    const std::vector<std::string>& knownDataTypes();
    const std::vector<std::string>& knownRequirements();

    // Columns that are always written to the output tree, whatever the config says
    // (needed to match nt events to the eventID_pass tree and to friend trees)
    const std::vector<std::string>& mandatoryOutputColumns();

    // Apply the output config to the dataframe's columns.
    //   allColumns:     RDF GetColumnNames()        (input branches, aliases and defined columns)
    //   definedColumns: RDF GetDefinedColumnNames() (Define / Redefine)
    //   notInSaveAll:   columns never included by the save_all_* switches (only by keep entries),
    //                   e.g. compatibility fallbacks such as Veto11 -> Veto10
    // Prints a WARNING for every keep/drop entry (for the current sample type) that matches no column.
    // Returns the selected columns in the order of allColumns.
    std::vector<std::string> selectOutputColumns(const std::vector<std::string>& allColumns,
                                                 const std::vector<std::string>& definedColumns,
                                                 const OutputColumnsConfig& config,
                                                 bool isMC,
                                                 const std::vector<std::string>& notInSaveAll = {});

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
        // An unquoted value starting with '!' is read by YAML as a "tag" (and the value is lost),
        // e.g. expression: !ExcludedTimes. Quoted values have tag "!", plain values "?".
        const std::string& tag = value.Tag();
        if (tag.size() > 1 && tag[0] == '!') {
            throw std::runtime_error(context + ": the value of '" + key + "' (line " + std::to_string(value.Mark().line + 1) +
                                     ") starts with '" + tag + "', which YAML reads as a tag. Put the value in quotes, e.g. \"" + tag + "\"");
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
