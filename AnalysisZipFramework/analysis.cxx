#include <iostream>
#include "Analysis.h"

#include <TFile.h>
#include <TTree.h>

#include "MessageService.hpp"
#include "GRLUtils.h"
#include "ConfigUtils.h"

#include <iostream>
#include <string>

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
            ERROR("Usage: ", argv[0], " --run <run_number> [--output <file>] [--file-config <yaml>] [--grl-config <yaml>] [--output-config <yaml>] [-j [n]] [--isMC] [--isAsimov] [--no-reduced-charge] [-v]");
            return 1;
        }
    }

    if (runNumber == -1) {
        ERROR("Error: --run <number> is required.");
        ERROR("Usage: ", argv[0], " --run <run_number> [--output <file>] [--file-config <yaml>] [--grl-config <yaml>] [--output-config <yaml>] [-j [n]] [--isMC] [--isAsimov] [--no-reduced-charge] [-v]");
        return 1;
    }

    INFO("Run number:  ", runNumber);
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

    INFO("File config: ", fileConfigPath);
    INFO("GRL config:  ", grlConfigPath);
    INFO("Output config: ", outputConfigPath);

    ConfigUtils::GRLConfig  grlConfig;
    ConfigUtils::FileConfig fileConfig;
    ConfigUtils::OutputColumnsConfig outputConfig;
    try {
        grlConfig    = ConfigUtils::readGRLConfig(grlConfigPath);
        fileConfig   = ConfigUtils::readFileConfig(fileConfigPath);
        outputConfig = ConfigUtils::readOutputColumnsConfig(outputConfigPath);
    } catch (const std::exception& e) {
        ERROR("Failed to read config: ", e.what());
        return 1;
    }

    if (fileConfig.count(runNumber) == 0) {
        ERROR("No file configuration found for run ", runNumber, " in ", fileConfigPath);
        return 1;
    }
    const std::vector<TString> mainFiles = GRLUtils::toTStringVector(fileConfig[runNumber].dataPaths);
    const std::vector<TString> auxFiles  = GRLUtils::toTStringVector(fileConfig[runNumber].waveformPaths);
    INFO("Found ", mainFiles.size(), " main files and ", auxFiles.size(), " aux files for run ", runNumber);

    Analysis analysis("nt", mainFiles);
    if (!auxFiles.empty() && useReducedCharge)
    {
        analysis.addAuxFiles("tree", auxFiles);
    }

    analysis.isMC = isMC;
    analysis.isAsimov = isAsimov;
    analysis.useReducedCharge = useReducedCharge;
    analysis.setRunNumbers({runNumber});
    analysis.setOutputColumns(outputConfig);
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