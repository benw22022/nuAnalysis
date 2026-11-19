#include <iostream>
#include "Analysis.h"

#include <TFile.h>
#include <TTree.h>

#include "MessageService.hpp"
#include "GRLUtils.h"
#include "ConfigUtils.h"
#include "InputUtils.h"

#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <optional>

int main(int argc, char* argv[]) {

    int         runNumber  = -1;
    std::string outputFile = "";
    bool        isMC       = false;
    bool        useReducedCharge = true;
    bool        verbose    = false;
    bool        useMT      = false;
    int         nThreads   = 0; // 0 = all available cores (ROOT::EnableImplicitMT() default)
    // Config files (paths relative to the working directory, normally the build directory, where
    // cmake copies the config/ folder). The analysis config lists the configs to use (see
    // ConfigUtils::readAnalysisConfig); the --*-config options replace its sections.
    const std::string defaultAnalysisConfig = "config/analyses/muon_neutrino.yaml";
    std::string analysisConfigPath = defaultAnalysisConfig;
    bool analysisConfigGiven = false;
    std::string fileConfigPath, grlConfigPath, outputConfigPath, cutsConfigPath, definitionsConfigPath, categoriesConfigPath;
    std::set<std::string> configsOnCommandLine;   // analysis config sections given as a command line option
    // Alternative to --run: take all .root files in these directories (file config not used)
    std::vector<std::string> inputDirs;
    std::vector<std::string> auxDirs;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--run" || arg == "-r") {
            if (i + 1 >= argc) {
                ERROR("Error: --run requires an argument.");
                return 1;
            }
            try {
                runNumber = std::stoi(argv[++i]);
            } catch (...) {
                ERROR("Error: --run value '", argv[i], "' is not a valid integer.");
                return 1;
            }

        } else if (arg == "--output" || arg == "-o") {
            if (i + 1 >= argc) {
                ERROR("Error: --output requires an argument.");
                return 1;
            }
            outputFile = argv[++i];

        } else if (arg == "--input-dir") {
            if (i + 1 >= argc) {
                ERROR("Error: --input-dir requires an argument.");
                return 1;
            }
            inputDirs.push_back(argv[++i]);

        } else if (arg == "--aux-dir") {
            if (i + 1 >= argc) {
                ERROR("Error: --aux-dir requires an argument.");
                return 1;
            }
            auxDirs.push_back(argv[++i]);

        } else if (arg == "--file-config") {
            if (i + 1 >= argc) {
                ERROR("Error: --file-config requires an argument.");
                return 1;
            }
            fileConfigPath = argv[++i];
            configsOnCommandLine.insert("file_configs");

        } else if (arg == "--grl-config") {
            if (i + 1 >= argc) {
                ERROR("Error: --grl-config requires an argument.");
                return 1;
            }
            grlConfigPath = argv[++i];
            configsOnCommandLine.insert("grl");

        } else if (arg == "--output-config") {
            if (i + 1 >= argc) {
                ERROR("Error: --output-config requires an argument.");
                return 1;
            }
            outputConfigPath = argv[++i];
            configsOnCommandLine.insert("output");

        } else if (arg == "--cuts-config") {
            if (i + 1 >= argc) {
                ERROR("Error: --cuts-config requires an argument.");
                return 1;
            }
            cutsConfigPath = argv[++i];
            configsOnCommandLine.insert("cuts");

        } else if (arg == "--definitions-config") {
            if (i + 1 >= argc) {
                ERROR("Error: --definitions-config requires an argument.");
                return 1;
            }
            definitionsConfigPath = argv[++i];
            configsOnCommandLine.insert("definitions");

        } else if (arg == "--analysis-config") {
            if (i + 1 >= argc) {
                ERROR("Error: --analysis-config requires an argument.");
                return 1;
            }
            analysisConfigPath = argv[++i];
            analysisConfigGiven = true;

        } else if (arg == "--categories-config") {
            if (i + 1 >= argc) {
                ERROR("Error: --categories-config requires an argument.");
                return 1;
            }
            categoriesConfigPath = argv[++i];
            configsOnCommandLine.insert("categories");

        } else if (arg == "--isMC") {
            isMC = true;
            INFO("Running in MC mode: GRL, BCID and trigger cuts will be skipped.");
        } else if (arg == "--isAsimov") {
            ERROR("Error: --isAsimov was removed. Use --isMC: the MC truth selection (signal / background) is now done with the MC categories (config/categories.yaml).");
            return 1;

        } else if (arg == "--no-reduced-charge") {
            useReducedCharge = false;
            INFO("Reduced charge disabled: aux files will not be loaded and the VetoNu veto will use the raw charge.");

        } else if (arg == "--verbose" || arg == "-v") {
            verbose = true;
            MessageService::Debug(true);
            INFO("Running in verbose mode.");

        } else if (arg == "-j") {
            useMT = true;

            // Peek at the next argument to see if it's an integer core count.
            // If there's no next argument, or it doesn't parse as an integer,
            // treat -j as a bare flag (use all available cores).
            if (i + 1 < argc) {
                std::string next = argv[i + 1];
                try {
                    size_t pos;
                    int    parsed = std::stoi(next, &pos);
                    if (pos == next.size()) {
                        // Whole string was a valid integer
                        nThreads = parsed;
                        ++i; // consume the argument
                    }
                } catch (...) {
                    // Not an integer - leave nThreads at 0, don't consume next arg
                }
            }

        } else {
            ERROR("Unknown argument: ", arg);
            ERROR("Usage: ", argv[0], " (--run <run_number> | --input-dir <dir> [--aux-dir <dir>]) [--output <file>] [--analysis-config <yaml>] [--file-config <yaml>] [--grl-config <yaml>] [--output-config <yaml>] [--definitions-config <yaml>] [--categories-config <yaml>] [--cuts-config <yaml>] [-j [n]] [--isMC] [--no-reduced-charge] [-v]");
            return 1;
        }
    }

    const bool useInputDirs = !inputDirs.empty();
    if (useInputDirs && runNumber != -1) {
        ERROR("Error: give either --run or --input-dir, not both.");
        return 1;
    }
    if (!auxDirs.empty() && !useInputDirs) {
        ERROR("Error: --aux-dir can only be used with --input-dir (with --run the aux files come from the file config).");
        return 1;
    }
    if (runNumber == -1 && !useInputDirs) {
        ERROR("Error: --run <number> or --input-dir <dir> is required.");
        ERROR("Usage: ", argv[0], " (--run <run_number> | --input-dir <dir> [--aux-dir <dir>]) [--output <file>] [--analysis-config <yaml>] [--file-config <yaml>] [--grl-config <yaml>] [--output-config <yaml>] [--definitions-config <yaml>] [--categories-config <yaml>] [--cuts-config <yaml>] [-j [n]] [--isMC] [--no-reduced-charge] [-v]");
        return 1;
    }

    if (useInputDirs) {
        for (const auto& d : inputDirs) INFO("Input directory: ", d);
        for (const auto& d : auxDirs)   INFO("Aux directory:   ", d);
    } else {
        INFO("Run number:  ", runNumber);
    }
    INFO("Output file: ", (outputFile.empty() ? "(none)" : outputFile));

    if (useMT) {
        if (nThreads > 0) {
            INFO("Enabling multithreading with ", nThreads, " threads.");
            ROOT::EnableImplicitMT(nThreads);
        } else {
            INFO("Enabling multithreading with all available cores.");
            ROOT::EnableImplicitMT();
        }
    }

    ConfigUtils::GRLConfig  grlConfig;
    ConfigUtils::FileConfig fileConfig;
    ConfigUtils::OutputColumnsConfig outputConfig;
    ConfigUtils::SelectionConfig selection;
    ConfigUtils::DefinitionsConfig definitions;
    ConfigUtils::CategoriesConfig categories;
    // Everything used to configure this job, saved in the output file (config tree)
    std::vector<Analysis::ConfigRecord> configRecord;
    {
        std::string commandLine;
        for (int i = 0; i < argc; ++i) commandLine += (i ? " " : "") + std::string(argv[i]);
        configRecord.push_back({"command_line", "", commandLine});
    }
    try {
        // Which config parts each section uses:
        //   1. the command line option (--cuts-config, ...) if given,
        //   2. otherwise the analysis config's section (default analysis config: config/analyses/muon_neutrino.yaml;
        //      a section missing from the analysis config means: no config of that type).
        if (!analysisConfigGiven) INFO("Analysis config: ", analysisConfigPath, " (default, no --analysis-config given)");
        const ConfigUtils::AnalysisConfig analysisConfig = ConfigUtils::readAnalysisConfig(analysisConfigPath);
        configRecord.push_back({"analysis_config", analysisConfig.path, analysisConfig.text});
        const std::map<std::string, std::string> pathOption{
            {"file_configs", fileConfigPath}, {"grl", grlConfigPath}, {"output", outputConfigPath},
            {"definitions", definitionsConfigPath}, {"categories", categoriesConfigPath}, {"cuts", cutsConfigPath}};
        const std::map<std::string, std::string> optionName{
            {"file_configs", "--file-config"}, {"grl", "--grl-config"}, {"output", "--output-config"},
            {"definitions", "--definitions-config"}, {"categories", "--categories-config"}, {"cuts", "--cuts-config"}};
        auto partsFor = [&](const std::string& section) -> std::vector<ConfigUtils::ConfigPart> {
            std::vector<ConfigUtils::ConfigPart> parts;
            std::string how;
            if (section == "file_configs" && useInputDirs) {
                how = "not used (--input-dir)";
            } else if (configsOnCommandLine.count(section)) {
                parts = {ConfigUtils::configFilePart(pathOption.at(section))};
                how = "from " + optionName.at(section) + " (overrides the analysis config)";
            } else if (const auto p = analysisConfig.section(section)) {
                parts = *p;
                how = "from the analysis config";
            } else {
                how = "none (not in the analysis config)";
            }
            INFO("Config ", section, ": ", ConfigUtils::describeParts(parts), "  [", how, "]");
            for (const auto& p : parts) configRecord.push_back({section, p.origin, p.text});
            return parts;
        };

        definitions  = ConfigUtils::readDefinitionsConfig(partsFor("definitions"));
        categories   = ConfigUtils::readCategoriesConfig(partsFor("categories"));
        // Before the cuts config: cuts and histograms can use the categories as data_type
        selection    = ConfigUtils::readSelectionConfig(partsFor("cuts"), ConfigUtils::categoryDataTypes(categories));
        grlConfig    = ConfigUtils::readGRLConfig(partsFor("grl"));
        fileConfig   = ConfigUtils::readFileConfig(partsFor("file_configs"));
        const auto outputParts = partsFor("output");
        outputConfig = ConfigUtils::readOutputColumnsConfig(outputParts);
        if (outputParts.empty()) {
            WARNING("No output columns config: the nt tree will only contain the columns that are always saved (run, eventID, ...).");
        }
        if (!isMC && grlConfig.grlJsons.empty()) {
            WARNING("No GRL: every event of this data run fails 'Good times' and the luminosity is set to -1.");
        }
    } catch (const std::exception& e) {
        ERROR("Failed to read config: ", e.what());
        return 1;
    }

    // ── Input files: from the file config (--run) or from directories (--input-dir) ──
    std::vector<TString> mainFiles, auxFiles;
    std::vector<int> runNumbers;
    if (useInputDirs) {
        try {
            std::vector<std::string> files, aux;
            for (const auto& d : inputDirs) {
                const auto f = InputUtils::listRootFiles(d);
                files.insert(files.end(), f.begin(), f.end());
            }
            for (const auto& d : auxDirs) {
                const auto f = InputUtils::listRootFiles(d);
                aux.insert(aux.end(), f.begin(), f.end());
            }
            runNumbers = InputUtils::runNumbersInFiles(files, "nt");
            mainFiles = GRLUtils::toTStringVector(files);
            auxFiles  = GRLUtils::toTStringVector(aux);
        } catch (const std::exception& e) {
            ERROR("Failed to set up the input files: ", e.what());
            return 1;
        }
        if (runNumbers.empty()) {
            ERROR("Could not determine any run number: all input files are empty.");
            return 1;
        }
        std::string runList;
        for (int r : runNumbers) runList += (runList.empty() ? "" : ", ") + std::to_string(r);
        INFO("Found ", mainFiles.size(), " input files (runs: ", runList, ") and ", auxFiles.size(), " aux files.");
        if (!auxFiles.empty() && runNumbers.size() > 1) {
            // The aux lookup is keyed on eventID only, so event IDs from different runs would clash
            ERROR("Error: --aux-dir needs input files from a single run, but found runs ", runList,
                  ". Use one --input-dir per run (separate jobs).");
            return 1;
        }
    } else {
        if (fileConfig.count(runNumber) == 0) {
            ERROR("No file configuration found for run ", runNumber, fileConfig.empty() ? " (no file config is used)" : " in the file config");
            return 1;
        }
        mainFiles  = GRLUtils::toTStringVector(fileConfig[runNumber].dataPaths);
        auxFiles   = GRLUtils::toTStringVector(fileConfig[runNumber].waveformPaths);
        runNumbers = {runNumber};
        INFO("Found ", mainFiles.size(), " main files and ", auxFiles.size(), " aux files for run ", runNumber);
    }

    Analysis analysis("nt", mainFiles);
    if (!auxFiles.empty() && useReducedCharge)
    {
        analysis.addAuxFiles("tree", auxFiles);
    }

    analysis.isMC = isMC;
    analysis.useReducedCharge = useReducedCharge;
    analysis.setRunNumbers(runNumbers);
    analysis.setRunFiles(fileConfig);   // description / is_nominal / lumi_sample for the meta tree
    analysis.setOutputColumns(outputConfig);
    analysis.setDefinitions(definitions);
    analysis.setSelection(selection);
    analysis.setCategories(categories);
    analysis.setConfigRecord(configRecord);
    // Catch exceptions so that buffered log messages are flushed and the job exits with a
    // non-zero code (an uncaught exception aborts before std::cout is flushed)
    try {
        analysis.setGRL(GRLUtils::toTStringVector(grlConfig.grlJsons), GRLUtils::toTStringVector(grlConfig.grlCsvs));
        analysis.Run(outputFile);
    } catch (const std::exception& e) {
        ERROR("Analysis failed: ", e.what());
        return 1;
    }

    INFO("Analysis completed successfully.");
    return 0;
    
}