#include "Analysis.h"
#include <algorithm>
#include <glob.h>
#include "TInterpreter.h"
#include "MessageService.hpp"
#include <set>
#include "GRLUtils.h"
#include <algorithm>


std::vector<TString> Analysis::ExpandAndSort(TString pattern) {
    glob_t globResult;
    glob(pattern.Data(), GLOB_TILDE, nullptr, &globResult);
    
    std::vector<TString> files;
    for (size_t i = 0; i < globResult.gl_pathc; ++i)
        files.emplace_back(globResult.gl_pathv[i]);
    
    globfree(&globResult);
    std::sort(files.begin(), files.end());
    return files;
}

Analysis::Analysis() {
    // Default constructor
}

Analysis::Analysis(TString mainFileTreeName, const std::vector<TString>& mainFiles) 
: m_mainFileTreeName(mainFileTreeName), m_mainFileNames(mainFiles)
{   
    addMainFiles(mainFileTreeName, mainFiles);
}

Analysis::Analysis(TString mainFileTreeName, TString auxFileTreeName, const std::vector<TString>& mainFiles, const std::vector<TString>& auxFiles) 
: m_mainFileTreeName(mainFileTreeName), m_auxFileTreeName(auxFileTreeName), m_mainFileNames(mainFiles), m_auxFileNames(auxFiles)
{
    addMainFiles(mainFileTreeName, mainFiles);
    addAuxFiles(auxFileTreeName, auxFiles);
}

Analysis::Analysis(TString mainFileTreeName, TString mainFiles) 
: m_mainFileTreeName(mainFileTreeName)
{   
    auto files = ExpandAndSort(mainFiles);
    m_mainFileNames = files;
    addMainFiles(mainFileTreeName, files);
}

Analysis::Analysis(TString mainFileTreeName, TString auxFileTreeName, TString mainFiles, TString auxFiles)
: m_mainFileTreeName(mainFileTreeName), m_auxFileTreeName(auxFileTreeName)
{
    auto mainFilesList = ExpandAndSort(mainFiles);
    auto auxFilesList = ExpandAndSort(auxFiles);
    m_mainFileNames = mainFilesList;
    m_auxFileNames = auxFilesList;
    addMainFiles(mainFileTreeName, mainFilesList);
    addAuxFiles(auxFileTreeName, auxFilesList);
}

void Analysis::addMainFiles(TString mainFileTreeName, std::vector<TString> mainFiles) 
{
    m_mainFileTreeName = mainFileTreeName;
    m_mainFileNames = mainFiles;
    m_mainChain = new TChain(mainFileTreeName);
    INFO("Adding ", mainFiles.size(), " main files to chain:");
    for (const auto& file : mainFiles) {
        INFO("Adding main file: ", file);
        m_mainChain->Add(file);
    }
    m_mainChainSet = true;
}

void Analysis::addMainFiles(TString mainFileTreeName, TString mainFiles) 
{   
    auto files = ExpandAndSort(mainFiles);
    addMainFiles(mainFileTreeName, files);
}

void Analysis::addAuxFiles(TString auxFileTreeName, TString auxFiles) 
{
    auto files = ExpandAndSort(auxFiles);
    addAuxFiles(auxFileTreeName, files);
}

void Analysis::addAuxFiles(TString auxFileTreeName, std::vector<TString> auxFiles) 
{

    if (!m_mainChain) {
        std::cerr << "Error: Main chain must be set up before adding aux files." << std::endl;
        return;
    }

    if (m_auxChainSet) {
        std::cerr << "Multiple aux chains not supported" << std::endl;
        return;
    }

    TChain* auxChain = new TChain(auxFileTreeName);
    INFO("Adding ", auxFiles.size(), " aux files to chain:");
    for (const auto& file : auxFiles) {
        INFO("Adding aux file: ", file);
        auxChain->Add(file);
    }
    m_auxChain = auxChain;
    m_auxChainSet = true;   
}

// This would be the simple way to do it if the friend tree approach worked, but it doesn't seem to be working with the current file structure.
//  Keeping this here for reference but implementing a manual map-lookup approach instead.
// I think the fact that the branch is called "event" in the aux files, which apparerently is a reserved name in ROOT, is causing the friend tree approach to fail.
// void Analysis::BuildDataFrame() {
//     if (!m_mainChainSet) {
//             std::cerr << "Error: Main chain is not set up. Please add main files before running the analysis." << std::endl;
//             return;
//         }

//     if (m_auxChainSet) {
//         m_mainChain->GetListOfFiles()->Sort();
//         m_auxChain->GetListOfFiles()->Sort();

//         m_mainChain->BuildIndex("run", "eventID");
        
//         m_auxChain->SetAlias("evtNum", "event");
//         m_auxChain->BuildIndex("run", "evtNum");
//         // m_auxChain->BuildIndex("run", "event");
//         m_mainChain->AddFriend(m_auxChain, "aux");
//     }

//     // Construct the dataframe now that the chain is fully configured
//     m_df = std::make_unique<ROOT::RDataFrame>(*m_mainChain);
//     m_node = *m_df;
        // m_node = m_node->Filter("aux.myVetoNu0_rawCharge35 < 40", "Reduced charge cut");

// }

void Analysis::setGRL(const std::vector<TString>& grlJsons, const std::vector<TString>& grlCSVs) {
    m_runLumiDict = GRLUtils::getRunNumberLumiDict(grlCSVs);
    m_excludedTimesCut = GRLUtils::makeExcludedTimesCut(grlJsons);
    m_goodTimesCut = GRLUtils::makeGoodTimesCut(grlJsons);
}



void Analysis::Define(std::string columnName, std::string expression, DataType dataType) {
    if (dataType == MC && !isMC) {
        INFO("Skipping Define for data: ", columnName);
        return;
    }
    if (dataType == DATA && isMC) {
        INFO("Skipping Define for MC: ", columnName);
        return;
    }

    m_node = m_node->Define(columnName, expression);
}

// Main setup function that builds the dataframe and sets up the aux chain if present
void Analysis::BuildDataFrame() {

    if (!m_mainChainSet) {
        ERROR("Error: Main chain not set up.");
        return;
    }

    // Allow shorter use of vecops functions in strings
    // e.g. DeltaPhi rather than ROOT::VecOps::DeltaPhi 
    gInterpreter->Declare("using namespace ROOT::VecOps;");

    // C++ defines (must not rely on anything defined below)
    gInterpreter->AddIncludePath(PROJECT_INCLUDE_DIR);
    gInterpreter->Declare("#include \"RDFDefines.h\"");

    m_df   = std::make_unique<ROOT::RDataFrame>(*m_mainChain);
    m_node = *m_df;

    // Aliases for older NTuples where the branch names were different
    auto columnNames = m_node->GetColumnNames();
    if (std::find(columnNames.begin(), columnNames.end(), "VetoSt10_raw_charge") != columnNames.end()) {
        m_node = m_node->Alias("VetoSt10_raw_charge", "Veto10_raw_charge");
        m_node = m_node->Alias("VetoSt20_raw_charge", "Veto20_raw_charge");
        m_node = m_node->Alias("VetoSt21_raw_charge", "Veto21_raw_charge");
        m_node = m_node->Alias("VetoSt10_status", "Veto10_status");
        m_node = m_node->Alias("VetoSt20_status", "Veto20_status");
        m_node = m_node->Alias("VetoSt21_status", "Veto21_status");
        m_node = m_node->Alias("VetoSt10_charge", "Veto10_charge");
        m_node = m_node->Alias("VetoSt20_charge", "Veto20_charge");
        m_node = m_node->Alias("VetoSt21_charge", "Veto21_charge");
    }

    if (m_auxChainSet) {
        // Build a lookup map from the aux chain manually
        // Key: {run, event}, Value: struct of aux quantities
        struct AuxData { float charge35_nu0; float charge35_nu1; };
        
        // auto auxMap = std::make_shared<std::unordered_map<Int_t, AuxData>>();
        auto auxMap = std::make_shared<std::unordered_map<Int_t, AuxData>>();

        Int_t  run, evt;
        float charge35_nu0, charge35_nu1;

        m_auxChain->SetBranchAddress("run",                   &run);
        m_auxChain->SetBranchAddress("event",                 &evt);
        m_auxChain->SetBranchAddress("myVetoNu0_rawCharge35", &charge35_nu0);
        m_auxChain->SetBranchAddress("myVetoNu1_rawCharge35", &charge35_nu1);

        Long64_t nAux = m_auxChain->GetEntries();
        if (nAux == 0) {
            ERROR("Warning: Aux chain has no entries. Check aux file paths and tree name.");
            throw std::runtime_error("Unable to open aux files.");
        }
        INFO("Loading ", nAux, " aux entries into lookup map...");
        for (Long64_t i = 0; i < nAux; ++i) {
            m_auxChain->GetEntry(i);
            Int_t key = getLookupKey(run, evt);

            if (auxMap->find(key) != auxMap->end()) {
                ERROR("Warning: Duplicate (run, event) pair in aux chain: (", run, ", ", evt, "). Overwriting previous entry.");
            }

            (*auxMap)[key] = {charge35_nu0, charge35_nu1};
        }

        INFO("Loaded ", auxMap->size(), " unique (run, event) pairs.");

        // Inject aux quantities as new columns via Define()
        // Capture auxMap by shared_ptr so it stays alive
        // Define a column which flags if an event had a good hit in either veto station or preshower - need a good hit to trust the reduced charge values from the aux file
        // Veto status 512 is what is recorded as a good hit on the second digitizer (CaloNu period specific)
        Define("GoodVeto20Hit",[](int status) { return !std::isnan(status) && (status & ~512) == 0; }, {"Veto20_status"});
        Define("GoodVeto21Hit", [](int status) { return !std::isnan(status) && (status & ~512) == 0; }, {"Veto21_status"});
        Define("GoodPreshower0Hit", [](int status) { return !std::isnan(status) && (status & ~512) == 0; }, {"Preshower0_status"}); 
        Define("GoodPreshower1Hit", [](int status) { return !std::isnan(status) && (status & ~512) == 0; }, {"Preshower1_status"});

        Define("BadVetoStatus", "Veto20_status == 528 || Veto21_status == 528", DATA);
        Define("GoodVetoNuStatus", "((VetoNu0_status == 0 || VetoNu0_status == 1) && (VetoNu1_status == 0 || VetoNu1_status == 1)) || (VetoNu0_status == 0 && VetoNu1_status == 16)");;
        Define("isCaloNuPeriod", "15821 <= run  && run <= 16924", DATA);
        Define("is2024Period", "run >= 1.2e4", DATA);
        Define("TimingOK", "(GoodVeto20Hit || GoodVeto21Hit || GoodPreshower0Hit || GoodPreshower1Hit)");
        Define("GoodScintillatorStatus", "TimingOK && GoodVetoNuStatus && !BadVetoStatus");
        Define("caloNuVeto2StatusBug", "(Veto20_status == 528 || Veto21_status == 528) && isCaloNuPeriod", DATA);
        Define("caloNuVetoNuStatusKeep", "GoodVetoNuStatus && isCaloNuPeriod", DATA);
        Define("caloNuStatusCleaning", "!isCaloNuPeriod || (!caloNuVeto2StatusBug && caloNuVetoNuStatusKeep)", DATA);

        Define("VetoNu0_reduced_charge",
            [this, auxMap](Int_t run, Int_t eventID, bool TimingOK, bool is2024Period, float VetoNu0_raw_charge) -> float {
                
                Int_t key = getLookupKey(run, eventID);
                bool lookup_success = auxMap->count(key);
                auto reduced_charge = -99999.f;
                
                if (lookup_success) {

                    auto it = auxMap->find(key);
                    reduced_charge = it->second.charge35_nu0;
                    
                    // If there's no good hit in the veto stations or preshower, use the original charge instead of the reduced charge
                    if (!TimingOK || std::isnan(reduced_charge) || (is2024Period && reduced_charge == 0 && VetoNu0_raw_charge > 30)){
                        // m_NVetoNu0_fallbacks++;
                        // return VetoNu0_raw_charge;
                        return -1234.f;
                    }
                    
                    return std::max(reduced_charge, 0.0f);
                }
                
                ERROR("Warning: Missing aux data for eventID ", eventID, " in run ", run, ". Setting reduced charge to -999.");
                m_NVetoNu0_missing_aux++;
                return reduced_charge;
                
            }, {"run", "eventID", "TimingOK", "is2024Period", "VetoNu0_raw_charge"});
        
        Define("VetoNu1_reduced_charge",
            [this, auxMap](Int_t run, Int_t eventID, bool TimingOK, bool is2024Period, float VetoNu1_raw_charge) -> float {

                Int_t key = getLookupKey(run, eventID);
                auto reduced_charge = -99999.f;
                
                bool lookup_success = auxMap->count(key);
                if (lookup_success) {
                    
                    auto it = auxMap->find(key);
                    reduced_charge = it->second.charge35_nu1;

                    // If there's no good hit in the veto stations or preshower, use the original charge instead of the reduced charge
                    if (!TimingOK || std::isnan(reduced_charge) || (is2024Period && reduced_charge == 0 && VetoNu1_raw_charge > 30)){
                        // m_NVetoNu1_fallbacks++;
                        // return VetoNu1_raw_charge; 
                        return -1234.f;
                    }

                    return std::max(reduced_charge, 0.0f);
                }

                ERROR("Warning: Missing aux data for eventID ", eventID, " in run ", run, ". Setting reduced charge to -999.");
                m_NVetoNu1_missing_aux++;
                return reduced_charge;    
                
                // return std::max(reduced_charge, 0.0f);
            }, {"run", "eventID", "TimingOK", "is2024Period", "VetoNu1_raw_charge"});

        Define("AuxLookupSuccess", [auxMap, this](Int_t run, Int_t eventID) -> bool {
            Int_t key = getLookupKey(run, eventID);
            return auxMap->count(key) > 0;
        }, {"run", "eventID"});

        Define("validReducedVetoNu0Charge", "AuxLookupSuccess && VetoNu0_reduced_charge >= 0", DATA);
        Define("validReducedVetoNu1Charge", "AuxLookupSuccess && VetoNu1_reduced_charge >= 0", DATA);

        Define("fallbackVetoNu0Charge",
            [this](bool validReducedVetoNu0Charge, float VetoNu0_raw_charge) -> float {
                if (!validReducedVetoNu0Charge) {
                    m_NVetoNu0_fallbacks++;
                    return VetoNu0_raw_charge;
                }
                return -1234.f; // Return -1234.f to indicate no fallback needed
            }, {"validReducedVetoNu0Charge", "VetoNu0_raw_charge"});
        

        Define("fallbackVetoNu1Charge",
            [this](bool validReducedVetoNu1Charge, float VetoNu1_raw_charge) -> float {
                if (!validReducedVetoNu1Charge) {
                    m_NVetoNu1_fallbacks++;
                    return VetoNu1_raw_charge;
                }
                return -1234.f; // Return -1234.f to indicate no fallback needed
            }, {"validReducedVetoNu1Charge", "VetoNu1_raw_charge"});

    } // End of aux chain handling


    // Definitions
    Define("Timing_charge_bottom", "Timing0_charge + Timing1_charge");
    Define("Timing_charge_top", "Timing2_charge + Timing3_charge");
    Define("Timing_charge_total", "Timing_charge_top + Timing_charge_bottom");

    Define("hitsTiming", "((Track_Y_atTrig[0] > 20 && Timing_charge_top > 20) || \
                        (Track_Y_atTrig[0] < -20 && Timing_charge_bottom > 20) || \
                        (Track_Y_atTrig[0] > -20 && Track_Y_atTrig[0] < 20 && Timing_charge_total > 20))");


    Define("LeadTrack_Idx", "ROOT::VecOps::ArgMax(Track_pz0)");
    Define("Track_rVetoNu","Radius(Track_X_atVetoNu, Track_Y_atVetoNu)");

    Define("Track_rVetoStation1", "pow(Track_X_atVetoStation1[LeadTrack_Idx]*Track_X_atVetoStation1[LeadTrack_Idx] + Track_Y_atVetoStation1[LeadTrack_Idx]*Track_Y_atVetoStation1[LeadTrack_Idx], 0.5)");
    Define("Track_rVetoStation2", "pow(Track_X_atVetoStation2[LeadTrack_Idx]*Track_X_atVetoStation2[LeadTrack_Idx] + Track_Y_atVetoStation2[LeadTrack_Idx]*Track_Y_atVetoStation2[LeadTrack_Idx], 0.5)");
    Define("Track_rIFT", "Radius(Track_X_atVetoStation2, Track_Y_atVetoStation2)");
    Define("Track_Theta", "Theta(Track_px0, Track_py0, Track_pz0)");
    Define("LeadTrack_pz0", "Track_pz0[LeadTrack_Idx] / 1000");
    Define("LeadTrack_Theta", "Track_Theta[LeadTrack_Idx] * 1000");
    Define("LeadTrack_nLayers", "Track_nLayers[LeadTrack_Idx]");
    Define("LeadTrack_nDoF", "Track_nDoF[LeadTrack_Idx]");
    Define("LeadTrack_Chi2", "Track_Chi2[LeadTrack_Idx]");
    Define("LeadTrack_Chi2_NDF", "Track_Chi2[LeadTrack_Idx] / Track_nDoF[LeadTrack_Idx]");

    Define("LeadTrack_rVetoNu", "Track_rVetoNu[LeadTrack_Idx]");
    Define("LeadTrack_r_atMaxRadius", "Track_r_atMaxRadius[LeadTrack_Idx]");
    Define("LeadTrack_rIFT", "Track_rIFT[LeadTrack_Idx]");

    Define("GoodTimes", m_goodTimesCut, DATA);
    Define("ExcludedTimes", m_excludedTimesCut, DATA);


    // This needs to go at the end of all the definitions, otherwise the aux columns won't be available for cuts
    m_eventIDNode = m_node;
}

void replaceAll(std::string& str, const std::string& from, const std::string& to) {
    if (from.empty()) return;
    size_t pos = 0;
    while ((pos = str.find(from, pos)) != std::string::npos) {
        str.replace(pos, from.length(), to);
        pos += to.length();
    }
}

void Analysis::applyCut(std::string cutExpression, std::string cutName, DataType dataType) {
    INFO("Applying cut: ", cutName, " (", cutExpression, ")");

    if (dataType == MC && !isMC) {
        INFO("Skipping cut for data: ", cutName);
        return;
    }
    if (dataType == DATA && isMC) {
        INFO("Skipping cut for MC: ", cutName);
        return;
    }

    std::string pass_cut_name = "passed_" + cutName;
    const std::vector<std::pair<std::string, std::string>> replacements = {
        {"<=", "leq"}, {">=", "geq"}, {"==", "eq"},
        {"<",  "lt"},  {">",  "gt"},
        {" ",  "_"}, {"&&", "and"}, {"||", "or"}, {"&", "and"}, {"|", "or"}, {"!", "not"},
        {"(", ""}, {")", ""}, {".", "_"}, {"/", "_"}, {"\\", "_"}, {"[", "_"}, {"]", "_"}, 
        {"-", "minus"}, {"+", "plus"}, {"*", "times"}, {"%", "percent"}, {"^", "caret"}
    };

    for (const auto& [from, to] : replacements) {
        replaceAll(pass_cut_name, from, to);
    }
    
    m_passedCutColNames.push_back(pass_cut_name);
    m_eventIDNode = m_eventIDNode->Define(pass_cut_name, cutExpression);
    
    m_node = m_node->Filter(cutExpression, cutName);

}

// Main analysis function that runs the analysis and applies cuts, then saves the output to a file if specified.
void Analysis::Run(TString outputFileName) {

    BuildDataFrame();

    if (!m_df) {
        ERROR("Error: DataFrame not built. Call BuildDataFrame() first.");
        throw std::runtime_error("DataFrame not built");
        return;
    }

    auto nEventsBeforeCuts = m_node->Count();

    // ── Cuts ──────────────
    applyCut("distanceToCollidingBCID == 0", "Colliding", DATA);
    applyCut("(inputBits & 0x8) == 0x8 || (inputBits & 0x10) == 0x10 || (inputBits & 0x20) == 0x20 || (inputBits & 0x40) == 0x40", "Trigger", DATA);

    DEBUG("Applying GRL good times cut: ", m_goodTimesCut);
    applyCut("GoodTimes", "Good times", DATA);

    if (m_excludedTimesCut != "") {
        DEBUG("Applying GRL excluded times cut: ", m_excludedTimesCut);
        applyCut("!ExcludedTimes", "Excluded times", DATA);
    }

    applyCut("AuxLookupSuccess", "Sanity cut to remove events with missing aux data", DATA);
    applyCut("VetoNu0_reduced_charge < 30", "VetoNu0 reduced charge < 30 pC");
    applyCut("VetoNu1_reduced_charge < 30", "VetoNu1 reduced charge < 30 pC");
    applyCut("fallbackVetoNu0Charge < 40", "VetoNu0 raw charge < 40 pC (fallback to raw charge if reduced charge invalid)", DATA);
    applyCut("fallbackVetoNu1Charge < 40", "VetoNu1 raw charge < 40 pC (fallback to raw charge if reduced charge invalid)", DATA);
    applyCut("caloNuStatusCleaning", "CaloNu status cleaning", DATA);
    applyCut("Veto20_charge > 40 && Veto21_charge > 40", "Veto20 and Veto21 charge > 40 pC");
    applyCut("((Track_Y_atTrig[LeadTrack_Idx] > 20 && Timing_charge_top > 20) || (Track_Y_atTrig[LeadTrack_Idx] < -20 && Timing_charge_bottom > 20) || (abs(Track_Y_atTrig[LeadTrack_Idx]) < 20 && Timing_charge_total > 20))", "Timing Station Charge > 20 pC");
    applyCut("Preshower0_charge > 2.5 && Preshower1_charge > 2.5", "Preshower Charge > 2.5 pC");
    applyCut("longTracks > 0", "At least one long track");
    applyCut("LeadTrack_pz0 > 100", "Track pz > 100 GeV");
    applyCut("LeadTrack_nLayers >= 7", "Leading track has at >= 7 layers");
    applyCut("LeadTrack_nDoF >= 9", "Track nDoF >= 9");
    applyCut("LeadTrack_Chi2_NDF < 15", "Leading track has chi2/ndof < 15");
    applyCut("LeadTrack_r_atMaxRadius < 95", "Track R at max radius < 95 mm");
    applyCut("LeadTrack_rIFT < 95", "Track R at IFT < 95 mm");
    applyCut("LeadTrack_rVetoNu < 120", "Track rVetoNu < 120 mm");
    applyCut("LeadTrack_Theta < 25", "Leading track theta < 25 mrad");
      

    // ── Book ALL actions before triggering any event loop ──────────────────
    auto cutReport = m_node->Report();
    auto runsCol   = m_node->Take<int>("run");

    if (outputFileName != "") {
        INFO("Saving snapshot to ", outputFileName, "...");

        auto opts = ROOT::RDF::RSnapshotOptions();
        opts.fMode = "RECREATE";
        auto columns = m_node->GetColumnNames();

        // ── SINGLE EVENT LOOP ───────────────────────────────────────────────
        m_node->Snapshot(m_mainFileTreeName, outputFileName, columns, opts);

        // ── Post-processing to save metadata and cutflow info ───────────────
        TFile *file = TFile::Open(outputFileName, "UPDATE");

        // ── Metadata tree (uses cached runsCol) ─────────────────────────────
        TTree *meta_tree = new TTree("meta", "Metadata tree");

        std::set<int> uniqueRuns(runsCol->begin(), runsCol->end());

        if (uniqueRuns.empty()) {
            if (nEventsBeforeCuts.GetValue() == 0) {
                WARNING("No events in the input file. The output file will be empty.");
            }
            else {
                WARNING("No events passed the cuts. Luminosity will be set to -1 (bug).");
            }
        } else {
            INFO("Unique runs in this file: ", uniqueRuns.size());
        }

        std::vector<int>   runBranch(uniqueRuns.begin(), uniqueRuns.end());
        std::vector<float> lumiBranch;
        for (const auto& run : uniqueRuns) {
            auto it = m_runLumiDict.find(run);
            lumiBranch.push_back(it != m_runLumiDict.end() ? it->second : -1.f);

            if (it == m_runLumiDict.end()) {
                WARNING("Run ", run, " not found in GRL! Setting lumi to -1.");
            }
        }

        float totalLumi = std::accumulate(lumiBranch.begin(), lumiBranch.end(), 0.f);
        INFO("Total integrated luminosity for this run: ", totalLumi, " /pb");

        meta_tree->Branch("run_number", &runBranch);
        meta_tree->Branch("lumi",       &lumiBranch);
        meta_tree->Fill();
        meta_tree->Write();
        INFO("Saved metadata tree with ", uniqueRuns.size(), " unique runs.");

        // ── Cutflow tree (uses cached cutReport) ────────────────────────────
        TTree *cutflow_tree = new TTree("cutflow", "Cutflow tree");
        std::string cut_name;
        ULong64_t   all, passed;
        double      efficiency;

        cutflow_tree->Branch("cut_name",   &cut_name);
        cutflow_tree->Branch("all",        &all);
        cutflow_tree->Branch("passed",     &passed);
        cutflow_tree->Branch("efficiency", &efficiency);

        for (auto&& cut : *cutReport) {
            cut_name   = cut.GetName();
            all        = cut.GetAll();
            passed     = cut.GetPass();
            efficiency = cut.GetEff();
            INFO("Cut: ", cut_name, ", All: ", all, ", Passed: ", passed, ", Efficiency: ", efficiency);
            cutflow_tree->Fill();
        }
        cutflow_tree->Write();
        file->Close();
        INFO("Wrote cutflow tree");


        // ── Event ID pass tree (for debugging) ────────────────────────────
        INFO("Writing eventID_pass tree for debugging (might be slow)...");
        opts.fMode = "UPDATE";
        m_passedCutColNames.push_back("run");
        m_passedCutColNames.push_back("eventID");
        m_eventIDNode->Snapshot("eventID_pass", outputFileName, m_passedCutColNames, opts);
        INFO("Wrote eventID_pass tree with ", m_eventIDNode->Count().GetValue(), " entries.");

    } else {
        cutReport->Print();
    }

    INFO("Total event loop runs: ", m_node->GetNRuns());
    if (m_node->GetNRuns() > 1) {
        WARNING("Event loop ran multiple times. This is inefficient.");
    }

    // Sanity check fallbacks and missing aux data
    // These go here, after event loop has run. If we put them before, they would be printed before the event loop runs and thus always show 0 fallbacks, which is misleading.
    if (m_auxChainSet) {
        INFO("Number of VetoNu0 reduced charge fallbacks: ", m_NVetoNu0_fallbacks.load());
        INFO("Number of VetoNu1 reduced charge fallbacks: ", m_NVetoNu1_fallbacks.load());
        if (m_NVetoNu0_missing_aux.load() > 0) {
            ERROR(m_NVetoNu0_missing_aux.load(), " events had missing VetoNu0 auxiliary data. This should never happen! Fellback to raw charge. Check aux files for missing data or mismatched event IDs.");
        }
        if (m_NVetoNu1_missing_aux.load() > 0) {
            ERROR(m_NVetoNu1_missing_aux.load(), " events had missing VetoNu1 auxiliary data. This should never happen! Fellback to raw charge. Check aux files for missing data or mismatched event IDs.");
        }
    }
}