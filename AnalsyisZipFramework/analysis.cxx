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

        } else {
            ERROR("Unknown argument: ", arg);
            ERROR("Usage: ", argv[0], " --run <run_number> [--output <file>]");
            return 1;
        }
    }

    if (runNumber == -1) {
        ERROR("Error: --run <number> is required.");
        ERROR("Usage: ", argv[0], " --run <run_number> [--output <file>]");
        return 1;
    }

    INFO("Run number:  ", runNumber);
    INFO("Output file: ", (outputFile.empty() ? "(none)" : outputFile));

    INFO("Run number: ", runNumber);
   
    GRLUtils::GRLConfig grlConfig = GRLUtils::readGRLConfig("config/config.json");
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
    analysis.addAuxFiles("tree", auxFiles);
    analysis.setGRL(grlConfig.grlJsons, grlConfig.grlCsvs);
    analysis.Run("output.root");

    return 0;
    
}