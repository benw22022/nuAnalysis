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
    for (const auto& file : mainFiles) {
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
    for (const auto& file : auxFiles) {
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
    // TODO: Fix hardcoded path
    // gInterpreter->Declare("#include \"/afs/cern.ch/user/b/bewilson/work/AnalyisZipFramework/AnalsyisZipFramework/include/RDFDefines.h\"");
    gInterpreter->AddIncludePath(PROJECT_INCLUDE_DIR);
    gInterpreter->Declare("#include \"RDFDefines.h\"");

    m_df   = std::make_unique<ROOT::RDataFrame>(*m_mainChain);
    m_node = *m_df;

    if (m_auxChainSet) {
        // Build a lookup map from the aux chain manually
        // Key: {run, event}, Value: struct of aux quantities
        struct AuxData { float charge35_nu0; float charge35_nu1; };
        
        auto auxMap = std::make_shared<std::unordered_map<uint64_t, AuxData>>();

        int  run, evt;
        float charge35_nu0, charge35_nu1;

        m_auxChain->SetBranchAddress("run",                   &run);
        m_auxChain->SetBranchAddress("event",                 &evt);
        m_auxChain->SetBranchAddress("myVetoNu0_rawCharge35", &charge35_nu0);
        m_auxChain->SetBranchAddress("myVetoNu1_rawCharge35", &charge35_nu1);

        Long64_t nAux = m_auxChain->GetEntries();
        if (nAux == 0) {
            std::cerr << "Warning: Aux chain has no entries." << std::endl;
        }
        INFO("Loading ", nAux, " aux entries into lookup map...");
        for (Long64_t i = 0; i < nAux; ++i) {
            m_auxChain->GetEntry(i);
            // Pack run+event into a single 64-bit key
            uint64_t key = (uint64_t)run << 32 | (uint32_t)evt;
            (*auxMap)[key] = {charge35_nu0, charge35_nu1};
        }

        INFO("Loaded ", auxMap->size(), " unique (run, event) pairs.");

        // Inject aux quantities as new columns via Define()
        // Capture auxMap by shared_ptr so it stays alive
        m_node = m_node->Define("reduced_charge35_nu0",
            [auxMap](int run, int eventID) -> float {
                uint64_t key = (uint64_t)run << 32 | (uint32_t)eventID;
                auto it = auxMap->find(key);
                return (it != auxMap->end()) ? it->second.charge35_nu0 : -999.f;
            }, {"run", "eventID"});
        
        m_node = m_node->Define("reduced_charge35_nu1",
            [auxMap](int run, int eventID) -> float {
                uint64_t key = (uint64_t)run << 32 | (uint32_t)eventID;
                auto it = auxMap->find(key);
                return (it != auxMap->end()) ? it->second.charge35_nu1 : -999.f;
            }, {"run", "eventID"});
    }

    // Aliases for older NTuples where the branch names were different
    auto columnNames = m_node->GetColumnNames();
    if (std::find(columnNames.begin(), columnNames.end(), "VetoSt10_raw_charge") != columnNames.end()) {
        m_node = m_node->Alias("VetoSt10_raw_charge", "Veto10_raw_charge");
        m_node = m_node->Alias("VetoSt20_raw_charge", "Veto20_raw_charge");
        m_node = m_node->Alias("VetoSt21_raw_charge", "Veto21_raw_charge");
    }

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
    
    m_node = m_node->Define("hitsVeto10", "Veto10_raw_charge > 40");
    m_node = m_node->Define("hitsVeto20", "Veto20_raw_charge > 40");
    m_node = m_node->Define("hitsVeto21", "Veto21_raw_charge > 40");

    m_node = m_node->Define("hitsTiming", "((Track_Y_atTrig[0] > 20 && Timing_charge_top > 20) || \
                                           (Track_Y_atTrig[0] < -20 && Timing_charge_bottom > 20) || \
                                           (Track_Y_atTrig[0] > -20 && Track_Y_atTrig[0] < 20 && Timing_charge_total > 20))");
    
    m_node = m_node->Define("hitsTiming0", "Timing0_status == 0");
    m_node = m_node->Define("hitsTiming1", "Timing1_status == 0");
    m_node = m_node->Define("hitsTiming2", "Timing2_status == 0");
    m_node = m_node->Define("hitsTiming3", "Timing3_status == 0");

    m_node = m_node->Define("vetoSaturated", "Veto10_raw_charge > 2500 || Veto20_raw_charge > 100 || Veto21_raw_charge > 100");
    
    m_node = m_node->Define("hitsPreshower0", "Preshower0_raw_charge > 2.5");
    m_node = m_node->Define("hitsPreshower1", "Preshower1_raw_charge > 2.5");

    m_node = m_node->Define("LeadTrack_Idx", "ROOT::VecOps::ArgMax(Track_pz0)");
    m_node = m_node->Define("Track_rVetoNu", "pow(Track_X_atVetoNu[LeadTrack_Idx]*Track_X_atVetoNu[LeadTrack_Idx] + Track_Y_atVetoNu[LeadTrack_Idx]*Track_Y_atVetoNu[LeadTrack_Idx], 0.5)");


    m_node = m_node->Define("Track_rVetoStation1", "pow(Track_X_atVetoStation1[LeadTrack_Idx]*Track_X_atVetoStation1[LeadTrack_Idx] + Track_Y_atVetoStation1[LeadTrack_Idx]*Track_Y_atVetoStation1[LeadTrack_Idx], 0.5)");
    m_node = m_node->Define("Track_rVetoStation2", "pow(Track_X_atVetoStation2[LeadTrack_Idx]*Track_X_atVetoStation2[LeadTrack_Idx] + Track_Y_atVetoStation2[LeadTrack_Idx]*Track_Y_atVetoStation2[LeadTrack_Idx], 0.5)");
    m_node = m_node->Define("Track_rIFT", "Radius(Track_X_atVetoStation2, Track_Y_atVetoStation2)");
    m_node = m_node->Define("Track_Theta", "Theta(Track_px0, Track_py0, Track_pz0)");
    m_node = m_node->Define("LeadTrack_pz0", "Track_pz0[LeadTrack_Idx] / 1000");
    m_node = m_node->Define("LeadTrack_Theta", "Track_Theta[LeadTrack_Idx]");
    m_node = m_node->Define("LeadTrack_Eta", "-ROOT::VecOps::log(ROOT::VecOps::tan(Track_Theta / 2))[LeadTrack_Idx]");

    m_node = m_node->Define("VetoNu_saturated", "VetoNu0_status == 4 || VetoNu1_status == 4");
    m_node = m_node->Define("VetoStation_saturated", "Veto10_status == 4 || Veto20_status == 4 || Veto21_status == 4");
    m_node = m_node->Define("timing_saturated", "Timing0_status == 4 || Timing1_status == 4 || Timing2_status == 4 || Timing3_status == 4");
    m_node = m_node->Define("preshower_saturated", "Preshower0_status == 4 || Preshower1_status == 4");
    m_node = m_node->Define("any_saturated", "VetoNu_saturated || VetoStation_saturated || timing_saturated || preshower_saturated");
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

    // ── Cuts ──────────────

    applyCut("distanceToCollidingBCID == 0", "Colliding");
    applyCut("(inputBits & 0x8) == 0x8 || (inputBits & 0x10) == 0x10 || (inputBits & 0x20) == 0x20 || (inputBits & 0x40) == 0x40", "Trigger");

    if (m_excludedTimesCut != "") {
        DEBUG("Applying GRL excluded times cut: ", m_excludedTimesCut);
        m_node = m_node->Define("ExcludedTimes", m_excludedTimesCut);
        applyCut("!ExcludedTimes", "Excluded times");
    }

    applyCut("reduced_charge35_nu0 < 40 && reduced_charge35_nu1 < 40", "Reduced charge cut");
    applyCut("reduced_charge35_nu0 > -999 && reduced_charge35_nu1 > -999", "Sanity check for aux lookup");
    applyCut("hitsVeto20 && hitsVeto21", "Veto20 and Veto21 charge > 40 pC");
    applyCut("((Track_Y_atTrig[LeadTrack_Idx] > 20 && Timing_charge_top > 20) || \
               (Track_Y_atTrig[LeadTrack_Idx] < -20 && Timing_charge_bottom > 20) || \
               (abs(Track_Y_atTrig[LeadTrack_Idx]) < 20 && Timing_charge_total > 20))", "Timing Station Charge");
    applyCut("Preshower0_charge > 2.5 && Preshower1_charge > 2.5", "Preshower Charge");
    applyCut("longTracks >= 1", "At least one long track");
    applyCut("Track_nLayers[LeadTrack_Idx] >= 7", "Leading track has at >= 7 layers");
    applyCut("Track_nDoF[LeadTrack_Idx] >= 9", "Track nDoF >= 9");
    applyCut("Track_Chi2[LeadTrack_Idx] / Track_nDoF[LeadTrack_Idx] < 15", "Leading track has chi2/ndof < 15");
    applyCut("Track_r_atMaxRadius[LeadTrack_Idx] < 95", "Track R at max radius < 95 mm");
    applyCut("Track_rIFT[LeadTrack_Idx] < 95", "Track R at IFT < 95 mm");
    applyCut("LeadTrack_Theta < 0.025", "Leading track theta < 0.025 rad");
    applyCut("Track_rVetoNu < 120", "Track rVetoNu < 120 mm");
    applyCut("LeadTrack_pz0 > 100", "Track pz > 100 GeV");

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

        std::vector<int>   runBranch(uniqueRuns.begin(), uniqueRuns.end());
        std::vector<float> lumiBranch;
        for (const auto& run : uniqueRuns) {
            auto it = m_runLumiDict.find(run);
            lumiBranch.push_back(it != m_runLumiDict.end() ? it->second : -1.f);
        }

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
}