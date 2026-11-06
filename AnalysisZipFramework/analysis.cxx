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

int main(int argc, char* argv[]) {

    int         runNumber  = -1;
    std::string outputFile = "";
    bool        isMC       = false;
    bool        isAsimov   = false;
    bool        useReducedCharge = true;
    bool        verbose    = false;
    bool        useMT      = false;
    int         nThreads   = 0; // 0 = all available cores (ROOT::EnableImplicitMT() default)
    // Config file paths (relative to the working directory, normally the build directory,
    // where cmake copies the config/ folder)
    std::string fileConfigPath = "config/file_config.yaml";
    std::string grlConfigPath  = "config/grl_config.yaml";
    std::string outputConfigPath = "config/output_columns.yaml";
    std::string cutsConfigPath   = "config/cuts.yaml";
    std::string definitionsConfigPath = "config/definitions.yaml";
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

        } else if (arg == "--grl-config") {
            if (i + 1 >= argc) {
                ERROR("Error: --grl-config requires an argument.");
                return 1;
            }
            grlConfigPath = argv[++i];

        } else if (arg == "--output-config") {
            if (i + 1 >= argc) {
                ERROR("Error: --output-config requires an argument.");
                return 1;
            }
            outputConfigPath = argv[++i];

        } else if (arg == "--cuts-config") {
            if (i + 1 >= argc) {
                ERROR("Error: --cuts-config requires an argument.");
                return 1;
            }
            cutsConfigPath = argv[++i];

        } else if (arg == "--definitions-config") {
            if (i + 1 >= argc) {
                ERROR("Error: --definitions-config requires an argument.");
                return 1;
            }
            definitionsConfigPath = argv[++i];

        } else if (arg == "--isMC") {
            isMC = true;
            INFO("Running in MC mode: GRL, BCID and trigger cuts will be skipped.");
        } else if (arg == "--isAsimov") {
            isAsimov = true;
            isMC = true; // Asimov mode implies MC
            INFO("Running in Asimov mode: GRL, BCID cuts will be skipped, no truth cuts will be applied to MC.");

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
            ERROR("Usage: ", argv[0], " (--run <run_number> | --input-dir <dir> [--aux-dir <dir>]) [--output <file>] [--file-config <yaml>] [--grl-config <yaml>] [--output-config <yaml>] [--definitions-config <yaml>] [--cuts-config <yaml>] [-j [n]] [--isMC] [--isAsimov] [--no-reduced-charge] [-v]");
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
        ERROR("Usage: ", argv[0], " (--run <run_number> | --input-dir <dir> [--aux-dir <dir>]) [--output <file>] [--file-config <yaml>] [--grl-config <yaml>] [--output-config <yaml>] [--definitions-config <yaml>] [--cuts-config <yaml>] [-j [n]] [--isMC] [--isAsimov] [--no-reduced-charge] [-v]");
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

    if (useInputDirs) INFO("File config: not used (--input-dir)");
    else              INFO("File config: ", fileConfigPath);
    INFO("GRL config:  ", grlConfigPath);
    INFO("Output config: ", outputConfigPath);
    INFO("Definitions config: ", definitionsConfigPath);
    INFO("Cuts config:   ", cutsConfigPath);

    ConfigUtils::GRLConfig  grlConfig;
    ConfigUtils::FileConfig fileConfig;
    ConfigUtils::OutputColumnsConfig outputConfig;
    ConfigUtils::SelectionConfig selection;
    ConfigUtils::DefinitionsConfig definitions;
    try {
        definitions  = ConfigUtils::readDefinitionsConfig(definitionsConfigPath);
        selection    = ConfigUtils::readSelectionConfig(cutsConfigPath);
        grlConfig    = ConfigUtils::readGRLConfig(grlConfigPath);
        if (!useInputDirs) fileConfig = ConfigUtils::readFileConfig(fileConfigPath);
        outputConfig = ConfigUtils::readOutputColumnsConfig(outputConfigPath);
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
            ERROR("No file configuration found for run ", runNumber, " in ", fileConfigPath);
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
    analysis.isAsimov = isAsimov;
    analysis.useReducedCharge = useReducedCharge;
    analysis.setRunNumbers(runNumbers);
    analysis.setOutputColumns(outputConfig);
    analysis.setDefinitions(definitions);
    analysis.setSelection(selection);
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