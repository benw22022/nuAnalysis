#include "ConfigUtils.h"
#include "MessageService.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <fnmatch.h>
#include <regex>

namespace ConfigUtils {

    namespace {
        std::string lineOf(const YAML::Node& n) { return " (line " + std::to_string(n.Mark().line + 1) + ")"; }
        std::string toUpper(std::string s) { std::transform(s.begin(), s.end(), s.begin(), ::toupper); return s; }
        std::string toLower(std::string s) { std::transform(s.begin(), s.end(), s.begin(), ::tolower); return s; }

        std::string readText(const std::string& path) {
            std::ifstream in(path);
            std::stringstream ss;
            ss << in.rdbuf();
            return ss.str();
        }

        // For "defined more than once" errors: where the first definition is
        std::string alsoIn(const std::string& first, const std::string& current) {
            return first != current ? " (also in " + first + ")" : " (twice in this file, or this file is listed more than once)";
        }

        // A config file must contain a map (or nothing)
        void requireMapOrEmpty(const ConfigPart& p) {
            if (p.isFile && !p.node.IsNull() && !p.node.IsMap()) {
                throw std::runtime_error(p.origin + ": expected a YAML map at the top level of the file");
            }
        }
    }

    YAML::Node loadYAML(const std::string& path) {
        if (!std::filesystem::exists(path)) {
            throw std::runtime_error("Config file not found: " + path);
        }
        try {
            return YAML::LoadFile(path);
        } catch (const YAML::Exception& e) {
            // e.what() includes the line and column of the problem
            std::string hint;
            if (std::string(e.what()).find("unknown escape character") != std::string::npos) {
                hint = " (in double quotes YAML reads a backslash as an escape, e.g. in LaTeX \\mu or \\nu: put such values in single quotes)";
            }
            throw std::runtime_error("Could not parse YAML config " + path + ": " + e.what() + hint);
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

    // ── Config parts ────────────────────────────────────────────────────────
    ConfigPart configFilePart(const std::string& path) {
        ConfigPart part;
        part.node   = loadYAML(path);
        part.origin = path;
        part.isFile = true;
        part.text   = readText(path);
        return part;
    }

    std::string describeParts(const std::vector<ConfigPart>& parts) {
        if (parts.empty()) return "(none)";
        std::string out;
        for (const auto& p : parts) out += (out.empty() ? "" : " + ") + (p.isFile ? p.origin : "<inline entry>");
        return out;
    }

    // ── GRL config ──────────────────────────────────────────────────────────
    GRLConfig readGRLConfig(const std::string& configPath) { return readGRLConfig({configFilePart(configPath)}); }

    GRLConfig readGRLConfig(const std::vector<ConfigPart>& parts) {
        GRLConfig config;
        for (const auto& p : parts) {
            INFO("Reading GRL config from ", p.origin, "...");
            if (!p.node.IsMap()) throw std::runtime_error(p.origin + ": expected a map with grl_jsons and/or grl_csvs" + lineOf(p.node));
            warnUnknownKeys(p.node, {"grl_jsons", "grl_csvs"}, p.origin);
            const auto jsons = getOptional<std::vector<std::string>>(p.node, "grl_jsons", {}, p.origin);
            const auto csvs  = getOptional<std::vector<std::string>>(p.node, "grl_csvs",  {}, p.origin);
            if (jsons.empty() && csvs.empty()) WARNING(p.origin, ": no grl_jsons or grl_csvs given");
            config.grlJsons.insert(config.grlJsons.end(), jsons.begin(), jsons.end());
            config.grlCsvs.insert(config.grlCsvs.end(), csvs.begin(), csvs.end());
        }
        INFO("GRL config: ", config.grlJsons.size(), " JSON files, ", config.grlCsvs.size(), " CSV files.");
        return config;
    }

    // ── File config ─────────────────────────────────────────────────────────
    FileConfig readFileConfig(const std::string& configPath) { return readFileConfig({configFilePart(configPath)}); }

    FileConfig readFileConfig(const std::vector<ConfigPart>& parts) {
        FileConfig fileConfig;
        std::map<int, std::string> origins;   // where each run was defined (for the duplicate error)
        for (const auto& p : parts) {
            INFO("Reading file config from ", p.origin, "...");
            requireMapOrEmpty(p);
            YAML::Node runs = p.node;           // inline entry: run number -> {data_paths, waveform_paths}
            if (p.isFile) {
                warnUnknownKeys(p.node, {"runs"}, p.origin);
                runs = p.node["runs"];
            }
            if (!runs || !runs.IsMap()) {
                throw std::runtime_error(p.origin + ": expected " + (p.isFile ? "a 'runs:' map of " : "a map of ") +
                                         "run number -> {data_paths, waveform_paths}");
            }

            for (const auto& kv : runs) {
                int runNumber;
                try {
                    runNumber = kv.first.as<int>();
                } catch (const YAML::Exception&) {
                    throw std::runtime_error(p.origin + ": run key '" + kv.first.as<std::string>() + "'" + lineOf(kv.first) +
                                             " is not an integer run number");
                }

                const std::string context = p.origin + ": run " + std::to_string(runNumber);
                if (fileConfig.count(runNumber)) {
                    throw std::runtime_error(context + lineOf(kv.first) + " is defined more than once" +
                                             alsoIn(origins[runNumber], p.origin));
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
                origins[runNumber] = p.origin;
            }
        }
        INFO("File config: ", fileConfig.size(), " runs.");
        return fileConfig;
    }

    // ── Output columns config ───────────────────────────────────────────────
    OutputColumnsConfig readOutputColumnsConfig(const std::string& configPath) {
        return readOutputColumnsConfig(std::vector<ConfigPart>{configFilePart(configPath)});
    }

    OutputColumnsConfig readOutputColumnsConfig(const std::vector<ConfigPart>& parts) {
        OutputColumnsConfig config;
        for (const auto& p : parts) {
            INFO("Reading output columns config from ", p.origin, "...");
            requireMapOrEmpty(p);
            YAML::Node nt = p.node;             // inline entry: the settings of the nt tree
            if (p.isFile) {
                warnUnknownKeys(p.node, {"nt"}, p.origin);
                nt = p.node["nt"];
            }
            if (!nt || !nt.IsMap()) {
                throw std::runtime_error(p.origin + (p.isFile ? ": expected an 'nt:' map (settings for the output nt tree)"
                                                              : ": expected the settings of the nt tree (save_all_input_columns, keep, ...)"));
            }
            const std::string context = p.origin + (p.isFile ? ": nt" : "");
            warnUnknownKeys(nt, {"save_all_input_columns", "save_all_defined_columns", "keep", "keep_mc", "keep_data", "drop"}, context);

            config.saveAllInputColumns   = config.saveAllInputColumns   || getOptional<bool>(nt, "save_all_input_columns",   false, context);
            config.saveAllDefinedColumns = config.saveAllDefinedColumns || getOptional<bool>(nt, "save_all_defined_columns", false, context);
            for (auto [key, list] : {std::pair{"keep", &config.keep}, {"keep_mc", &config.keepMC},
                                     {"keep_data", &config.keepData}, {"drop", &config.drop}}) {
                const auto entries = getOptional<std::vector<std::string>>(nt, key, {}, context);
                list->insert(list->end(), entries.begin(), entries.end());
            }
        }
        config.sourcePath = describeParts(parts);

        INFO("Output columns config: save_all_input_columns=", config.saveAllInputColumns,
             ", save_all_defined_columns=", config.saveAllDefinedColumns,
             ", keep: ", config.keep.size(), " (+", config.keepMC.size(), " MC, +", config.keepData.size(), " data)",
             ", drop: ", config.drop.size());
        return config;
    }

    // ── Selection config: cuts and histograms ─────────────────────────────
    const std::vector<std::string>& knownDataTypes() {
        static const std::vector<std::string> v{"ALL", "DATA", "MC"};
        return v;
    }

    const std::vector<std::string>& knownRequirements() {
        static const std::vector<std::string> v{"reduced_charge", "aux_reduced_charge", "native_reduced_charge", "no_reduced_charge"};
        return v;
    }

    namespace {

        // ALL, DATA, MC (any case, returned upper case) or, if allowed, an MC category
        // data type: signal / background / other (any case, returned lower case) or a category name (exact)
        std::string readDataType(const YAML::Node& node, const std::string& context,
                                 const std::vector<std::string>& categoryTypes = {}) {
            const std::string raw = getOptional<std::string>(node, "data_type", "ALL", context);
            const auto& ok = knownDataTypes();
            if (std::find(ok.begin(), ok.end(), toUpper(raw)) != ok.end()) return toUpper(raw);
            if (toUpper(raw) == "ASIMOV") {
                throw std::runtime_error(context + ": data_type ASIMOV" + lineOf(node["data_type"]) + " was removed: use data_type: MC, which now "
                                         "applies to all MC (the MC truth selection is done with the MC categories in config/categories.yaml)");
            }
            const std::string low = toLower(raw);
            const bool generic = low == "signal" || low == "background" || low == "other";
            if (generic && std::find(categoryTypes.begin(), categoryTypes.end(), low) != categoryTypes.end()) return low;
            if (std::find(categoryTypes.begin(), categoryTypes.end(), raw) != categoryTypes.end()) return raw;
            std::string allowed = "ALL, DATA, MC";
            if (!categoryTypes.empty()) {
                allowed += " or an MC category data type (";
                for (std::size_t i = 0; i < categoryTypes.size(); ++i) allowed += (i ? ", " : "") + categoryTypes[i];
                allowed += ")";
            } else if (generic) {
                allowed += " (signal, background, other and category names are only allowed on cuts and histograms, with MC categories in the categories config)";
            }
            throw std::runtime_error(context + ": data_type '" + raw + "'" + lineOf(node["data_type"]) + " must be one of " + allowed);
        }

        std::vector<std::string> readRequires(const YAML::Node& node, const std::string& context) {
            auto reqs = getOptional<std::vector<std::string>>(node, "requires", {}, context);
            const auto& ok = knownRequirements();
            for (const auto& r : reqs) {
                if (std::find(ok.begin(), ok.end(), r) == ok.end()) {
                    std::string list;
                    for (const auto& k : ok) list += (list.empty() ? "" : ", ") + k;
                    throw std::runtime_error(context + ": unknown requirement '" + r + "'" + lineOf(node["requires"]) + ". Allowed: " + list);
                }
            }
            return reqs;
        }

        AxisConfig readAxis(const YAML::Node& node, const std::string& context) {
            if (!node.IsMap()) throw std::runtime_error(context + ": expected a map {variable, bins, min, max} or {variable, edges}" + lineOf(node));
            warnUnknownKeys(node, {"variable", "bins", "min", "max", "edges"}, context);
            AxisConfig axis;
            axis.variable = getRequired<std::string>(node, "variable", context);
            const bool fixed = node["bins"] || node["min"] || node["max"];
            const bool edges = static_cast<bool>(node["edges"]);
            if (fixed && edges) throw std::runtime_error(context + ": give EITHER bins/min/max OR edges, not both" + lineOf(node));
            if (!fixed && !edges) throw std::runtime_error(context + ": missing binning: give bins/min/max or edges" + lineOf(node));
            if (edges) {
                axis.edges = getRequired<std::vector<double>>(node, "edges", context);
                if (axis.edges.size() < 2) throw std::runtime_error(context + ": edges needs at least 2 values" + lineOf(node["edges"]));
                for (std::size_t i = 1; i < axis.edges.size(); ++i) {
                    if (!(axis.edges[i] > axis.edges[i - 1])) throw std::runtime_error(context + ": edges must be strictly increasing" + lineOf(node["edges"]));
                }
            } else {
                axis.nBins = getRequired<int>(node, "bins", context);
                axis.min   = getRequired<double>(node, "min", context);
                axis.max   = getRequired<double>(node, "max", context);
                if (axis.nBins <= 0)       throw std::runtime_error(context + ": bins must be > 0" + lineOf(node["bins"]));
                if (!(axis.max > axis.min)) throw std::runtime_error(context + ": max must be greater than min" + lineOf(node["max"]));
            }
            return axis;
        }

        std::vector<HistogramConfig> readHistograms(const YAML::Node& list, const std::string& context, std::set<std::string>& usedNames,
                                                    const std::vector<std::string>& categoryTypes) {
            std::vector<HistogramConfig> out;
            if (!list || list.IsNull()) return out;
            if (!list.IsSequence()) throw std::runtime_error(context + ": 'histograms' must be a list" + lineOf(list));
            for (const auto& node : list) {
                if (!node.IsMap()) throw std::runtime_error(context + ": each histogram must be a map with name, x (and optionally y)" + lineOf(node));
                HistogramConfig h;
                h.name = getRequired<std::string>(node, "name", context + ": histogram" + lineOf(node));
                const std::string hctx = context + ": histogram '" + h.name + "'";
                warnUnknownKeys(node, {"name", "title", "data_type", "requires", "x", "y", "split"}, hctx);
                if (!usedNames.insert(h.name).second) throw std::runtime_error(hctx + lineOf(node) + " is defined more than once (histogram names must be unique)");
                h.title    = getOptional<std::string>(node, "title", h.name, hctx);
                h.dataType = readDataType(node, hctx, categoryTypes);
                h.requirements = readRequires(node, hctx);
                h.split    = getOptional<bool>(node, "split", true, hctx);
                if (!node["x"]) throw std::runtime_error(hctx + ": missing required key 'x'" + lineOf(node));
                h.x = readAxis(node["x"], hctx + ": x");
                if (node["y"]) h.y = readAxis(node["y"], hctx + ": y");
                out.push_back(h);
            }
            return out;
        }
    }

    SelectionConfig readSelectionConfig(const std::string& configPath, const std::vector<std::string>& categoryTypes) {
        return readSelectionConfig(std::vector<ConfigPart>{configFilePart(configPath)}, categoryTypes);
    }

    SelectionConfig readSelectionConfig(const std::vector<ConfigPart>& parts, const std::vector<std::string>& categoryTypes) {
        SelectionConfig config;
        config.sourcePath = describeParts(parts);
        std::set<std::string> histNames;                 // unique across all parts
        std::map<std::string, std::string> cutOrigins;   // cut name -> where it was defined
        for (const auto& p : parts) {
            INFO("Reading selection (cuts and histograms) config from ", p.origin, "...");
            requireMapOrEmpty(p);
            YAML::Node cuts = p.node;                    // inline entry: <cut name>: {expression, ...}
            if (p.isFile) {
                warnUnknownKeys(p.node, {"Cuts", "Histograms"}, p.origin);
                const auto hists = readHistograms(p.node["Histograms"], p.origin + ": Histograms", histNames, categoryTypes);
                config.histograms.insert(config.histograms.end(), hists.begin(), hists.end());
                cuts = p.node["Cuts"];
            }
            if (!cuts || cuts.IsNull()) continue;
            if (!cuts.IsMap()) throw std::runtime_error(p.origin + ": " + (p.isFile ? "'Cuts' must be " : "expected ") +
                                                        "a map of <cut name>: {expression, ...}" + lineOf(cuts));
            for (const auto& kv : cuts) {   // yaml-cpp keeps the order of the file
                CutConfig cut;
                cut.name = kv.first.as<std::string>();
                const std::string ctx = p.origin + ": cut '" + cut.name + "'";
                if (cutOrigins.count(cut.name)) {
                    throw std::runtime_error(ctx + lineOf(kv.first) + " is defined more than once" +
                                             alsoIn(cutOrigins[cut.name], p.origin));
                }
                cutOrigins[cut.name] = p.origin;
                const YAML::Node& c = kv.second;
                if (!c.IsMap()) throw std::runtime_error(ctx + ": expected a map with at least 'expression'" + lineOf(c));
                warnUnknownKeys(c, {"expression", "data_type", "requires", "histograms", "n_minus_1"}, ctx);
                cut.expression = getRequired<std::string>(c, "expression", ctx);
                if (cut.expression.find_first_not_of(" \t") == std::string::npos) {
                    throw std::runtime_error(ctx + ": expression is empty" + lineOf(c["expression"]));
                }
                cut.dataType   = readDataType(c, ctx, categoryTypes);
                cut.requirements   = readRequires(c, ctx);
                cut.histograms = readHistograms(c["histograms"], ctx, histNames, categoryTypes);
                cut.nMinus1    = readHistograms(c["n_minus_1"], ctx + " (n_minus_1)", histNames, categoryTypes);
                config.cuts.push_back(cut);
            }
        }

        INFO("Selection config: ", config.cuts.size(), " cuts, ", histNames.size(), " histograms (",
             config.histograms.size(), " before the first cut).");
        return config;
    }

    // ── Definitions config ──────────────────────────────────────────────────
    DefinitionsConfig readDefinitionsConfig(const std::string& configPath) {
        return readDefinitionsConfig(std::vector<ConfigPart>{configFilePart(configPath)});
    }

    DefinitionsConfig readDefinitionsConfig(const std::vector<ConfigPart>& parts) {
        DefinitionsConfig config;
        config.sourcePath = describeParts(parts);
        static const std::regex identifier("^[A-Za-z_][A-Za-z0-9_]*$");
        std::map<std::string, std::string> origins;   // name -> where it was defined
        for (const auto& p : parts) {
            INFO("Reading definitions config from ", p.origin, "...");
            requireMapOrEmpty(p);
            YAML::Node defs = p.node;                 // inline entry: <column name>: {expression, ...}
            if (p.isFile) {
                warnUnknownKeys(p.node, {"Definitions"}, p.origin);
                defs = p.node["Definitions"];
                if (!defs || defs.IsNull()) {
                    WARNING(p.origin, ": no 'Definitions' found, no columns will be defined from this file.");
                    continue;
                }
            }
            if (!defs.IsMap()) throw std::runtime_error(p.origin + ": " + (p.isFile ? "'Definitions' must be " : "expected ") +
                                                        "a map of <column name>: {expression, ...}" + lineOf(defs));

            for (const auto& kv : defs) {   // yaml-cpp keeps the order of the file
                DefinitionConfig d;
                d.name   = kv.first.as<std::string>();
                d.origin = p.origin;
                const std::string ctx = p.origin + ": definition '" + d.name + "'";
                if (!std::regex_match(d.name, identifier)) {
                    throw std::runtime_error(ctx + lineOf(kv.first) + ": the name must be a valid C++ identifier (letters, digits, '_'; not starting with a digit), because expressions refer to columns by name");
                }
                if (origins.count(d.name)) {
                    throw std::runtime_error(ctx + lineOf(kv.first) + " is defined more than once" +
                                             alsoIn(origins[d.name], p.origin));
                }
                origins[d.name] = p.origin;
                const YAML::Node& c = kv.second;
                if (!c.IsMap()) throw std::runtime_error(ctx + ": expected a map with at least 'expression'" + lineOf(c));
                warnUnknownKeys(c, {"expression", "data_type", "requires"}, ctx);
                d.expression = getRequired<std::string>(c, "expression", ctx);
                if (d.expression.find_first_not_of(" \t\n") == std::string::npos) {
                    throw std::runtime_error(ctx + ": expression is empty" + lineOf(c["expression"]));
                }
                d.dataType     = readDataType(c, ctx);
                d.requirements = readRequires(c, ctx);
                config.definitions.push_back(d);
            }
        }
        INFO("Definitions config: ", config.definitions.size(), " definitions.");
        return config;
    }

    // ── MC categories config ────────────────────────────────────────────────
    CategoriesConfig readCategoriesConfig(const std::string& configPath) {
        return readCategoriesConfig(std::vector<ConfigPart>{configFilePart(configPath)});
    }

    CategoriesConfig readCategoriesConfig(const std::vector<ConfigPart>& parts) {
        CategoriesConfig config;
        config.sourcePath = describeParts(parts);
        static const std::regex identifier("^[A-Za-z_][A-Za-z0-9_]*$");
        // Data type keywords and output directory names
        static const std::vector<std::string> reserved{"ALL", "DATA", "MC", "SIGNAL", "BACKGROUND", "OTHER", "N_MINUS_1"};
        std::map<std::string, std::string> origins;   // name -> where it was defined
        std::size_t nSignal = 0;
        for (const auto& p : parts) {
            INFO("Reading MC categories config from ", p.origin, "...");
            requireMapOrEmpty(p);
            YAML::Node cats = p.node;                 // inline entry: <category name>: {expression, type, ...}
            if (p.isFile) {
                warnUnknownKeys(p.node, {"Categories"}, p.origin);
                cats = p.node["Categories"];
                if (!cats || cats.IsNull()) {
                    INFO(p.origin, ": no 'Categories' in this file.");
                    continue;
                }
            }
            if (!cats.IsMap()) throw std::runtime_error(p.origin + ": " + (p.isFile ? "'Categories' must be " : "expected ") +
                                                        "a map of <category name>: {expression, type, ...}" + lineOf(cats));

            for (const auto& kv : cats) {   // yaml-cpp keeps the order of the file
                CategoryConfig c;
                c.name   = kv.first.as<std::string>();
                c.origin = p.origin;
                const std::string ctx = p.origin + ": category '" + c.name + "'";
                if (!std::regex_match(c.name, identifier)) {
                    throw std::runtime_error(ctx + lineOf(kv.first) + ": the name must be a valid C++ identifier (letters, digits, '_'; not starting with a digit), "
                                             "it is used for the flag column category_<name> and as a directory name in the output");
                }
                if (std::find(reserved.begin(), reserved.end(), toUpper(c.name)) != reserved.end()) {
                    throw std::runtime_error(ctx + lineOf(kv.first) + ": '" + c.name + "' is a reserved name (ALL, DATA, MC, signal, background, other, n_minus_1)");
                }
                if (origins.count(c.name)) {
                    throw std::runtime_error(ctx + lineOf(kv.first) + " is defined more than once" +
                                             alsoIn(origins[c.name], p.origin));
                }
                origins[c.name] = p.origin;
                const YAML::Node& v = kv.second;
                if (!v.IsMap()) throw std::runtime_error(ctx + ": expected a map with at least 'expression' and 'type'" + lineOf(v));
                warnUnknownKeys(v, {"expression", "type", "latex", "colour"}, ctx);
                c.expression = getRequired<std::string>(v, "expression", ctx);
                if (c.expression.find_first_not_of(" \t\n") == std::string::npos) {
                    throw std::runtime_error(ctx + ": expression is empty" + lineOf(v["expression"]));
                }
                c.type = toLower(getRequired<std::string>(v, "type", ctx));
                if (c.type != "signal" && c.type != "background") {
                    throw std::runtime_error(ctx + ": type '" + c.type + "'" + lineOf(v["type"]) + " must be signal or background");
                }
                if (c.type == "signal") ++nSignal;
                c.latex  = getOptional<std::string>(v, "latex", c.name, ctx);
                c.colour = getOptional<std::string>(v, "colour", "", ctx);
                // In double quotes YAML reads \t, \n, \b, ... as escapes, e.g. "$\nu_\tau$" -> newline + "u_" + tab + "au"
                if (std::any_of(c.latex.begin(), c.latex.end(), [](char ch) { return static_cast<unsigned char>(ch) < 0x20; })) {
                    throw std::runtime_error(ctx + ": latex" + lineOf(v["latex"]) + " contains a tab or newline. In double quotes YAML reads \\t, \\n, \\b, ... "
                                             "as escape sequences (e.g. \\tau, \\nu, \\bar). Use single quotes: latex: 'CC $\\nu_\\tau$'");
                }
                config.categories.push_back(c);
            }
        }
        // Categories are combined into a 64-bit mask (bit i = category i) to check that they do not overlap
        if (config.categories.size() > 63) throw std::runtime_error(config.sourcePath + ": at most 63 categories are supported");
        if (config.categories.empty()) {
            INFO("No MC categories: histograms and cutflows will not be split by MC category.");
        } else {
            INFO("MC categories config: ", config.categories.size(), " categories (", nSignal, " signal, ",
                 config.categories.size() - nSignal, " background).");
        }
        return config;
    }

    // ── Analysis config ─────────────────────────────────────────────────────
    const std::vector<std::string>& analysisConfigSections() {
        static const std::vector<std::string> v{"file_configs", "grl", "output", "definitions", "categories", "cuts"};
        return v;
    }

    std::optional<std::vector<ConfigPart>> AnalysisConfig::section(const std::string& name) const {
        const auto it = sections.find(name);
        if (it == sections.end()) return std::nullopt;
        return it->second;
    }

    AnalysisConfig readAnalysisConfig(const std::string& configPath) {
        INFO("Reading analysis config from ", configPath, "...");
        const YAML::Node root = loadYAML(configPath);
        AnalysisConfig config;
        config.path = configPath;
        config.text = readText(configPath);
        if (root.IsNull()) {
            WARNING(configPath, ": the analysis config is empty, no configs will be used.");
            return config;
        }
        if (!root.IsMap()) throw std::runtime_error(configPath + ": expected a map of <section>: [config files and inline entries]");

        // Config files are looked up relative to the directory of the analysis config
        const std::filesystem::path dir = std::filesystem::path(configPath).parent_path();
        const auto& known = analysisConfigSections();
        std::string knownList;
        for (const auto& k : known) knownList += (knownList.empty() ? "" : ", ") + k;

        for (const auto& kv : root) {
            const std::string key = kv.first.as<std::string>();
            const std::string name = toLower(key);
            const std::string ctx = configPath + ": " + key;
            // An unknown (e.g. misspelt) section is an error: it would otherwise be ignored, and with it its configs
            if (std::find(known.begin(), known.end(), name) == known.end()) {
                throw std::runtime_error(ctx + lineOf(kv.first) + ": unknown section. Allowed (any case): " + knownList);
            }
            if (config.sections.count(name)) throw std::runtime_error(ctx + lineOf(kv.first) + ": section given more than once");

            const YAML::Node& list = kv.second;
            std::vector<ConfigPart> parts;
            if (!list.IsNull()) {
                if (!list.IsSequence()) {
                    throw std::runtime_error(ctx + lineOf(list) + ": expected a list of config files and/or inline entries (each starting with '- ')");
                }
                for (std::size_t i = 0; i < list.size(); ++i) {
                    const YAML::Node& item = list[i];
                    if (item.IsScalar()) {
                        std::filesystem::path file(item.as<std::string>());
                        if (file.is_relative()) file = dir / file;
                        file = file.lexically_normal();
                        if (!std::filesystem::exists(file)) {
                            throw std::runtime_error(ctx + lineOf(item) + ": config file '" + item.as<std::string>() + "' not found (looked for " +
                                                     file.string() + "; relative paths are relative to the directory of the analysis config)");
                        }
                        parts.push_back(configFilePart(file.string()));
                    } else if (item.IsMap()) {
                        ConfigPart part;
                        part.node   = item;
                        part.origin = configPath + ": " + key + " entry " + std::to_string(i + 1) + " (inline, line " + std::to_string(item.Mark().line + 1) + ")";
                        part.isFile = false;
                        YAML::Emitter out;
                        out << item;
                        part.text = out.c_str();
                        parts.push_back(part);
                    } else {
                        throw std::runtime_error(ctx + lineOf(item) + ": each entry must be a config file name or an inline map of entries");
                    }
                }
            }
            config.sections[name] = parts;
        }
        INFO("Read analysis config: sections ", [&] {
            std::string s;
            for (const auto& [n, p] : config.sections) s += (s.empty() ? "" : ", ") + n + " (" + std::to_string(p.size()) + ")";
            return s.empty() ? std::string("(none)") : s;
        }(), ".");
        return config;
    }

    std::vector<std::string> categoryDataTypes(const CategoriesConfig& config) {
        if (config.categories.empty()) return {};
        std::vector<std::string> types{"signal", "background", "other"};
        for (const auto& c : config.categories) types.push_back(c.name);
        return types;
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
