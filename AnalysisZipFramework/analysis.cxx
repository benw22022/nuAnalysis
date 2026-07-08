#include <iostream>
#include "Analysis.h"

#include <TFile.h>
#include <TTree.h>

#include "MessageService.hpp"
#include "GRLUtils.h"

#include <iostream>
#include <string>

int main(int argc, char* argv[]) {

    int         runNumber  = -1;
    std::string outputFile = "";
    bool        isMC       = false;
    bool        verbose    = false;
    bool        useMT      = false;
    int         nThreads   = 0; // 0 = all available cores (ROOT::EnableImplicitMT() default)

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

        } else if (arg == "--isMC") {
            isMC = true;
            INFO("Running in MC mode: GRL, BCID and trigger cuts will be skipped.");

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
            ERROR("Usage: ", argv[0], " --run <run_number> [--output <file>] [-j [n]]");
            return 1;
        }
    }

    if (runNumber == -1) {
        ERROR("Error: --run <number> is required.");
        ERROR("Usage: ", argv[0], " --run <run_number> [--output <file>] [-j [n]]");
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

    GRLUtils::GRLConfig grlConfig = GRLUtils::readGRLConfig("config/grl_config.json");
    GRLUtils::FileConfig fileConfig = GRLUtils::parseFileConfig("config/file_config.json");
    
    std::vector<TString> mainFiles, auxFiles;
    if (fileConfig.count(runNumber) == 0) {
        ERROR("No file configuration found for run ", runNumber);
        return 1;
    } else {
        mainFiles = GRLUtils::toTStringVector(fileConfig[runNumber].first);
        auxFiles = GRLUtils::toTStringVector(fileConfig[runNumber].second);
        INFO("Found ", mainFiles.size(), " main files and ", auxFiles.size(), " aux files for run ", runNumber);
    }


    Analysis analysis("nt", mainFiles);
    if (!auxFiles.empty())
    {
        analysis.addAuxFiles("tree", auxFiles);
    }

    analysis.isMC = isMC;
    analysis.setGRL(grlConfig.grlJsons, grlConfig.grlCsvs);
    analysis.Run(outputFile);

    return 0;
    
}