#include <iostream>
#include "Analysis.h"

#include <TFile.h>
#include <TTree.h>

#include "MessageService.hpp"
#include "GRLUtils.h"

int main() {
    // ROOT::EnableImplicitMT(4);

    // MessageService::Info("Starting analysis...");
    INFO("Starting analysis...");

    #include <unistd.h>
    #include <stdio.h>
    #include <linux/limits.h>
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("Current working dir: %s\n", cwd);
    } else {
        perror("getcwd() error");
        return 1;
    }
   

    GRLUtils::GRLConfig grlConfig = GRLUtils::readGRLConfig("config/config.json");

    Analysis analysis("nt", "/eos/experiment/faser/data0/phys/2024_back/r0022/015838/*.root");
    analysis.addAuxFiles("tree", "/eos/home-b/bewilson/El9CalypsoForWaveForm/WaveformsCaloNuPeriod/015838/*.root");
    analysis.setGRL(grlConfig.grlJsons, grlConfig.grlCsvs);
    analysis.Run("output.root");

    return 0;
    
}