#include "Analysis.h"
#include <algorithm>
#include <glob.h>
#include "TInterpreter.h"
#include "MessageService.hpp"
#include <set>
#include "GRLUtils.h"

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
}


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

            if (auxMap->find(evt) != auxMap->end()) {
                ERROR("Warning: Duplicate (run, event) pair in aux chain: (", run, ", ", evt, "). Overwriting previous entry.");
            }

            (*auxMap)[evt] = {charge35_nu0, charge35_nu1};
        }

        INFO("Loaded ", auxMap->size(), " unique (run, event) pairs.");

        // Inject aux quantities as new columns via Define()
        // Capture auxMap by shared_ptr so it stays alive
        // Define a column which flags if an event had a good hit in either veto station or preshower - need a good hit to trust the reduced charge values from the aux file
        // Veto status 512 is what is recorded as a good hit on the second digitizer (CaloNu period specific)
        m_node = m_node->Define("GoodVeto20Hit",[](int status) { return (status & ~512) == 0; }, {"Veto20_status"});
        m_node = m_node->Define("GoodVeto21Hit", [](int status) { return (status & ~512) == 0; }, {"Veto21_status"});
        m_node = m_node->Define("GoodPreshower0Hit", [](int status) { return (status & ~512) == 0; }, {"Preshower0_status"}); 
        m_node = m_node->Define("GoodPreshower1Hit", [](int status) { return (status & ~512) == 0; }, {"Preshower1_status"});
        m_node = m_node->Define("GoodVetoOrPSHit", "GoodVeto20Hit || GoodVeto21Hit || GoodPreshower0Hit || GoodPreshower1Hit");
        m_node = m_node->Define("BadVetoStatus", "Veto20_status == 528 || Veto21_status == 528");
        m_node = m_node->Define("GoodVetoNuStatus", "((VetoNu0_status == 0 || VetoNu0_status == 1) && (VetoNu1_status == 0 || VetoNu1_status == 1)) || (VetoNu0_status == 0 && VetoNu1_status == 16)");
        m_node = m_node->Define("GoodScintillatorStatus", "GoodVetoOrPSHit && GoodVetoNuStatus && !BadVetoStatus");

        m_node = m_node->Define("VetoNu0_reduced_charge",
            [this, auxMap](Int_t run, Int_t eventID, bool GoodScintillatorStatus, float VetoNu0_raw_charge) -> float {
                
                // Lookup the reduced charge from the aux map
                auto it = auxMap->find(eventID);
                float reduced_charge = (it != auxMap->end()) ? it->second.charge35_nu0 : -999.;

                // If there's no good hit in the veto stations or preshower, use the original charge instead of the reduced charge
                if (!GoodScintillatorStatus || std::isnan(reduced_charge) || (reduced_charge == 0 && VetoNu0_raw_charge > 30)){
                    m_NVetoNu0_fallbacks++;
                    return VetoNu0_raw_charge; 
                }

                if (reduced_charge == -999.) {
                    m_NVetoNu0_missing_aux++;
                    m_NVetoNu0_fallbacks++;
                    // return reduced_charge;
                    return VetoNu0_raw_charge; // fallback to raw charge if aux data is missing, to avoid losing events with missing aux data which may still be salvageable with the original charge value
                }

                return reduced_charge;
            }, {"run", "eventID", "GoodScintillatorStatus", "VetoNu0_raw_charge"});
        
        m_node = m_node->Define("VetoNu1_reduced_charge",
            [this, auxMap](Int_t run, Int_t eventID, bool GoodScintillatorStatus, float VetoNu1_raw_charge) -> float {

                // Lookup the reduced charge from the aux map
                auto it = auxMap->find(eventID);
                float reduced_charge = (it != auxMap->end()) ? it->second.charge35_nu1 : -999.;

                // If there's no good hit in the veto stations or preshower, use the original charge instead of the reduced charge
                if (!GoodScintillatorStatus || std::isnan(reduced_charge) || (reduced_charge == 0 && VetoNu1_raw_charge > 30)){
                    m_NVetoNu1_fallbacks++;
                    
                    return VetoNu1_raw_charge; 
                }
                if (reduced_charge == -999.) {
                    m_NVetoNu1_missing_aux++;
                    m_NVetoNu1_fallbacks++;
                    // return reduced_charge; 
                    return VetoNu1_raw_charge; // fallback to raw charge if aux data is missing, to avoid losing events with missing aux data which may still be salvageable with the original charge value
                }
                return reduced_charge;
                
            }, {"run", "eventID", "GoodScintillatorStatus", "VetoNu1_raw_charge"});
    } // End of aux chain handling


    // Definitions
    m_node = m_node->Define("Track_nHits", "Track_nDoF + 5");        
    m_node = m_node->Define("Track_chi2_per_dof", "Track_Chi2/Track_nDoF");
    m_node = m_node->Define("Track_pz_charge0", "Track_pz0 * Track_charge");
    m_node = m_node->Define("Track_theta_x1", "asin(Track_px1/Track_p1)");
    m_node = m_node->Define("Track_theta_y1", "asin(Track_py1/Track_p1)");
    m_node = m_node->Define("Track_pt0", "sqrt(Track_px0*Track_px0 + Track_py0*Track_py0)");
    m_node = m_node->Define("Track_theta0", "asin(Track_pt0/Track_p0)");
    m_node = m_node->Define("Track_phi0", "acos(Track_px0/Track_pt0)");
    m_node = m_node->Define("Track_eta0", "-log(tan(Track_theta0/2))");
    m_node = m_node->Define("Track_theta_x0", "asin(Track_px0/Track_p0)");
    m_node = m_node->Define("Track_theta_y0", "asin(Track_py0/Track_p0)");
    m_node = m_node->Define("Track_theta_x0_pos", "Track_theta_x0[Track_charge > 0]");
    m_node = m_node->Define("Track_theta_x0_neg", "Track_theta_x0[Track_charge < 0]");
    m_node = m_node->Define("Track_theta_y0_pos", "Track_theta_y0[Track_charge > 0]");
    m_node = m_node->Define("Track_theta_y0_neg", "Track_theta_y0[Track_charge < 0]");
    m_node = m_node->Define("Track_x0_pos", "Track_x0[Track_charge > 0]");
    m_node = m_node->Define("Track_x0_neg", "Track_x0[Track_charge < 0]");
    m_node = m_node->Define("Track_y0_pos", "Track_y0[Track_charge > 0]");
    m_node = m_node->Define("Track_y0_neg", "Track_y0[Track_charge < 0]");

    m_node = m_node->Define("Timing_charge_bottom", "Timing0_raw_charge + Timing1_raw_charge");
    m_node = m_node->Define("Timing_charge_top", "Timing2_raw_charge + Timing3_raw_charge");
    m_node = m_node->Define("Timing_charge_total", "Timing_charge_top + Timing_charge_bottom");
    
    m_node = m_node->Define("hitsVetoNu0", "VetoNu0_raw_charge > 40");
    m_node = m_node->Define("hitsVetoNu1", "VetoNu1_raw_charge > 40");

    m_node = m_node->Define("goodVetoNuStatus", "(VetoNu0_status == 0 || VetoNu0_status == 512) && (VetoNu1_status == 0 || VetoNu1_status == 512)");
    
    m_node = m_node->Define("hitsVeto10", "Veto10_charge > 40");
    m_node = m_node->Define("hitsVeto20", "Veto20_charge > 40");
    m_node = m_node->Define("hitsVeto21", "Veto21_charge > 40");

    m_node = m_node->Define("hitsTiming", "((Track_Y_atTrig[0] > 20 && Timing_charge_top > 20) || \
                                           (Track_Y_atTrig[0] < -20 && Timing_charge_bottom > 20) || \
                                           (Track_Y_atTrig[0] > -20 && Track_Y_atTrig[0] < 20 && Timing_charge_total > 20))");
    
    m_node = m_node->Define("hitsTiming0", "Timing0_status == 0");
    m_node = m_node->Define("hitsTiming1", "Timing1_status == 0");
    m_node = m_node->Define("hitsTiming2", "Timing2_status == 0");
    m_node = m_node->Define("hitsTiming3", "Timing3_status == 0");

    m_node = m_node->Define("vetoSaturated", "Veto10_raw_charge > 2500 || Veto20_raw_charge > 100 || Veto21_raw_charge > 100");
    
    m_node = m_node->Define("hitsPreshower0", "Preshower0_charge > 2.5");
    m_node = m_node->Define("hitsPreshower1", "Preshower1_charge > 2.5");

    m_node = m_node->Define("LeadTrack_Idx", "ROOT::VecOps::ArgMax(Track_pz0)");
    m_node = m_node->Define("Track_rVetoNu","Radius(Track_X_atVetoNu, Track_Y_atVetoNu)");

    m_node = m_node->Define("Track_rVetoStation1", "pow(Track_X_atVetoStation1[LeadTrack_Idx]*Track_X_atVetoStation1[LeadTrack_Idx] + Track_Y_atVetoStation1[LeadTrack_Idx]*Track_Y_atVetoStation1[LeadTrack_Idx], 0.5)");
    m_node = m_node->Define("Track_rVetoStation2", "pow(Track_X_atVetoStation2[LeadTrack_Idx]*Track_X_atVetoStation2[LeadTrack_Idx] + Track_Y_atVetoStation2[LeadTrack_Idx]*Track_Y_atVetoStation2[LeadTrack_Idx], 0.5)");
    m_node = m_node->Define("Track_rIFT", "Radius(Track_X_atVetoStation2, Track_Y_atVetoStation2)");
    m_node = m_node->Define("Track_Theta", "Theta(Track_px0, Track_py0, Track_pz0)");
    m_node = m_node->Define("LeadTrack_pz0", "Track_pz0[LeadTrack_Idx] / 1000");
    m_node = m_node->Define("LeadTrack_Theta", "Track_Theta[LeadTrack_Idx]");
    m_node = m_node->Define("LeadTrack_Eta", "-ROOT::VecOps::log(ROOT::VecOps::tan(Track_Theta / 2))[LeadTrack_Idx]");
    m_node = m_node->Define("LeadTrack_nLayers", "Track_nLayers[LeadTrack_Idx]");
    m_node = m_node->Define("LeadTrack_nDoF", "Track_nDoF[LeadTrack_Idx]");
    m_node = m_node->Define("LeadTrack_Chi2", "Track_Chi2[LeadTrack_Idx]");
    
    m_node = m_node->Define("LeadTrack_rVetoNu", "Track_rVetoNu[LeadTrack_Idx]");
    m_node = m_node->Define("LeadTrack_r_atMaxRadius", "Track_r_atMaxRadius[LeadTrack_Idx]");
    m_node = m_node->Define("LeadTrack_rIFT", "Track_rIFT[LeadTrack_Idx]");

    m_node = m_node->Define("VetoNu_saturated", "VetoNu0_status == 4 || VetoNu1_status == 4");
    m_node = m_node->Define("VetoStation_saturated", "Veto10_status == 4 || Veto20_status == 4 || Veto21_status == 4");
    m_node = m_node->Define("timing_saturated", "Timing0_status == 4 || Timing1_status == 4 || Timing2_status == 4 || Timing3_status == 4");
    m_node = m_node->Define("preshower_saturated", "Preshower0_status == 4 || Preshower1_status == 4");
    m_node = m_node->Define("any_saturated", "VetoNu_saturated || VetoStation_saturated || timing_saturated || preshower_saturated");

    m_node = m_node->Define("VetoNuHit", "inputBits & 4");
    m_node = m_node->Define("VetoNuHitNext", "inputBitsNext & 4");
}

void Analysis::applyCut(std::string cutExpression, std::string cutName) {
    INFO("Applying cut: ", cutName, " (", cutExpression, ")");
    m_node = m_node->Filter(cutExpression, cutName);
}

void Analysis::Run(TString outputFileName) {

    BuildDataFrame();

    if (!m_df) {
        ERROR("Error: DataFrame not built. Call BuildDataFrame() first.");
        throw std::runtime_error("DataFrame not built");
        return;
    }

    auto nEventsBeforeCuts = m_node->Count();

    // ── Cuts ──────────────

    if (!isMC) {
        applyCut("distanceToCollidingBCID == 0", "Colliding");
        // applyCut("VetoNuHit || VetoNuHitNext", "VetoNu triggered in this or next BCID");
    
        if (m_excludedTimesCut != "") {
            DEBUG("Applying GRL excluded times cut: ", m_excludedTimesCut);
            m_node = m_node->Define("ExcludedTimes", m_excludedTimesCut);
            applyCut("!ExcludedTimes", "Excluded times");
        }
    }
    
    // applyCut("hitsVetoNu0 || hitsVetoNu1", "VetoNu hit"); 
    applyCut("preshower_saturated", "Preshower0_status == 4 || Preshower1_status == 4");
      

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