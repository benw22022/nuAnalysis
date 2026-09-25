#include "Analysis.h"
#include <algorithm>
#include <glob.h>
#include "TInterpreter.h"
#include "MessageService.hpp"
#include <set>
#include <regex>
#include <filesystem>
#include <numeric>
#include <cmath>
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
    m_grlTimes = std::make_shared<const GRLUtils::GRLTimes>(GRLUtils::readGRLTimes(grlJsons));
}

namespace {
    // Define GoodTimes / ExcludedTimes as compiled lookups. Templated on the type of the eventTime
    // branch, because RDataFrame needs the exact column type for a compiled (non-JIT) Define.
    template <typename TTime>
    void defineGRLTimeColumnsImpl(Analysis& analysis, std::shared_ptr<const GRLUtils::GRLTimes> grl) {
        analysis.Define("GoodTimes",
            [grl](Int_t run, TTime eventTime) { return grl->stable.contains(run, static_cast<long long>(eventTime)); },
            {"run", "eventTime"}, DATA);
        // Always defined (false for every event if the GRL has no excluded periods)
        analysis.Define("ExcludedTimes",
            [grl](Int_t run, TTime eventTime) { return grl->excluded.contains(run, static_cast<long long>(eventTime)); },
            {"run", "eventTime"}, DATA);
    }
}

void Analysis::defineGRLTimeColumns() {
    if (isMC) return;  // GRL time cuts are data only
    if (!m_grlTimes) {
        throw std::runtime_error("GRL not set: call Analysis::setGRL() before Run()");
    }
    for (int run : m_runNumbers) {
        if (!m_grlTimes->stable.hasRun(run)) {
            WARNING("Run ", run, " has no stable periods in the GRL: all its events will fail the 'Good times' cut.");
        }
    }
    const std::string timeType = m_node->GetColumnType("eventTime");
    if      (timeType == "Int_t"     || timeType == "int")                defineGRLTimeColumnsImpl<Int_t>(*this, m_grlTimes);
    else if (timeType == "UInt_t"    || timeType == "unsigned int")       defineGRLTimeColumnsImpl<UInt_t>(*this, m_grlTimes);
    else if (timeType == "Long64_t"  || timeType == "long long")          defineGRLTimeColumnsImpl<Long64_t>(*this, m_grlTimes);
    else if (timeType == "ULong64_t" || timeType == "unsigned long long") defineGRLTimeColumnsImpl<ULong64_t>(*this, m_grlTimes);
    else throw std::runtime_error("Unsupported type for the eventTime column: " + timeType);
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

    INFO("Defining column: ", columnName, " with expression: ", expression);

    m_node = m_node->Define(columnName, expression);
}

const bool Analysis::isColumnDefined(const std::string& columnName) {
    auto columnNames = m_node->GetColumnNames();
    return std::find(columnNames.begin(), columnNames.end(), columnName) != columnNames.end();
}

// ── Selection helpers ───────────────────────────────────────────────────────

// data_type semantics (see config/cuts.yaml):
//   ALL:    every sample
//   DATA:   data only
//   MC:     MC, but not in Asimov mode (truth selection)
//   ASIMOV: all MC, including Asimov mode
bool Analysis::appliesTo(DataType dataType) const {
    switch (dataType) {
        case ALL:    return true;
        case DATA:   return !isMC;
        case MC:     return isMC && !isAsimov;
        case ASIMOV: return isMC;
    }
    return true;
}

DataType Analysis::parseDataType(const std::string& dataType) {
    if (dataType == "ALL")    return ALL;
    if (dataType == "DATA")   return DATA;
    if (dataType == "MC")     return MC;
    if (dataType == "ASIMOV") return ASIMOV;
    throw std::runtime_error("Unknown data_type '" + dataType + "'");
}

bool Analysis::requirementsMet(const std::vector<std::string>& requirements, std::string& why) const {
    auto sourceName = [this]() -> std::string {
        switch (m_reducedChargeSource) {
            case ReducedChargeSource::Aux:    return "aux files";
            case ReducedChargeSource::Native: return "input NTuple";
            case ReducedChargeSource::None:   return "none (--no-reduced-charge)";
        }
        return "?";
    };
    for (const auto& r : requirements) {
        bool ok = true;
        if      (r == "reduced_charge")        ok = m_reducedChargeSource != ReducedChargeSource::None;
        else if (r == "aux_reduced_charge")    ok = m_reducedChargeSource == ReducedChargeSource::Aux;
        else if (r == "native_reduced_charge") ok = m_reducedChargeSource == ReducedChargeSource::Native;
        else if (r == "no_reduced_charge")     ok = m_reducedChargeSource == ReducedChargeSource::None;
        else throw std::runtime_error("Unknown requirement '" + r + "'");
        if (!ok) {
            why = "requires " + r + ", reduced charge source is " + sourceName();
            return false;
        }
    }
    return true;
}

// Book a 1D or 2D histogram at the current point of the selection
void Analysis::bookHistogram(const ConfigUtils::HistogramConfig& cfg) {
    if (!appliesTo(parseDataType(cfg.dataType))) {
        DEBUG("Skipping histogram ", cfg.name, " (data_type ", cfg.dataType, ")");
        ++m_nHistSkipped;
        return;
    }
    std::string why;
    if (!requirementsMet(cfg.requirements, why)) {
        DEBUG("Skipping histogram ", cfg.name, " (", why, ")");
        ++m_nHistSkipped;
        return;
    }
    for (const auto* axis : {&cfg.x, cfg.y ? &*cfg.y : nullptr}) {
        if (axis && !isColumnDefined(axis->variable)) {
            WARNING("Histogram '", cfg.name, "': column '", axis->variable, "' does not exist in the dataframe. Histogram not booked.");
            ++m_nHistSkipped;
            return;
        }
    }

    const auto& x = cfg.x;
    if (!cfg.y) {
        DEBUG("Booking 1D histogram ", cfg.name, " of ", x.variable);
        const auto model = x.hasEdges()
            ? ROOT::RDF::TH1DModel(cfg.name.c_str(), cfg.title.c_str(), static_cast<int>(x.edges.size()) - 1, x.edges.data())
            : ROOT::RDF::TH1DModel(cfg.name.c_str(), cfg.title.c_str(), x.nBins, x.min, x.max);
        m_histResults.push_back(m_node->Histo1D(model, x.variable));
        return;
    }

    const auto& y = *cfg.y;
    DEBUG("Booking 2D histogram ", cfg.name, " of ", y.variable, " vs ", x.variable);
    const char* n = cfg.name.c_str();
    const char* ti = cfg.title.c_str();
    const int nxe = static_cast<int>(x.edges.size()) - 1, nye = static_cast<int>(y.edges.size()) - 1;
    ROOT::RDF::TH2DModel model;
    if      (!x.hasEdges() && !y.hasEdges()) model = ROOT::RDF::TH2DModel(n, ti, x.nBins, x.min, x.max, y.nBins, y.min, y.max);
    else if ( x.hasEdges() && !y.hasEdges()) model = ROOT::RDF::TH2DModel(n, ti, nxe, x.edges.data(), y.nBins, y.min, y.max);
    else if (!x.hasEdges() &&  y.hasEdges()) model = ROOT::RDF::TH2DModel(n, ti, x.nBins, x.min, x.max, nye, y.edges.data());
    else                                     model = ROOT::RDF::TH2DModel(n, ti, nxe, x.edges.data(), nye, y.edges.data());
    m_histResults.push_back(m_node->Histo2D(model, x.variable, y.variable));
}

// Define the columns of the definitions config, in file order
void Analysis::applyDefinitions() {
    if (!m_definitions) {
        throw std::runtime_error("No definitions config set: call Analysis::setDefinitions() before Run()");
    }
    // Position of each definition in the file, to catch expressions using a definition from further down
    const auto& defs = m_definitions->definitions;
    std::unordered_map<std::string, std::size_t> position;
    for (std::size_t i = 0; i < defs.size(); ++i) position[defs[i].name] = i;
    const std::regex identifier(R"([A-Za-z_][A-Za-z0-9_]*)");

    std::size_t nDefined = 0, nSkipped = 0;
    for (std::size_t i = 0; i < defs.size(); ++i) {
        const auto& d = defs[i];
        if (!appliesTo(parseDataType(d.dataType))) {
            DEBUG("Skipping definition ", d.name, " (data_type ", d.dataType, ")");
            ++nSkipped;
            continue;
        }
        std::string why;
        if (!requirementsMet(d.requirements, why)) {
            DEBUG("Skipping definition ", d.name, " (", why, ")");
            ++nSkipped;
            continue;
        }
        if (isColumnDefined(d.name)) {
            throw std::runtime_error(m_definitions->sourcePath + ": definition '" + d.name +
                                     "' already exists as a column (input branch or built-in definition). Please use a different name.");
        }
        for (auto it = std::sregex_iterator(d.expression.begin(), d.expression.end(), identifier); it != std::sregex_iterator(); ++it) {
            const std::string token = it->str();
            auto pos = position.find(token);
            if (pos != position.end() && pos->second > i && !isColumnDefined(token)) {
                throw std::runtime_error(m_definitions->sourcePath + ": definition '" + d.name + "' uses '" + token +
                                         "', which is defined further down the file. Definitions are created in file order: move '" +
                                         token + "' above '" + d.name + "'.");
            }
        }
        DEBUG("Defining ", d.name, " = ", d.expression);
        m_node = m_node->Define(d.name, d.expression);
        ++nDefined;
    }
    INFO("Defined ", nDefined, " columns from ", m_definitions->sourcePath, " (", nSkipped,
         " skipped for this sample / reduced charge source; use -v for details).");
}

// Apply the cuts and book the histograms of the selection config, in file order
void Analysis::applySelection() {
    if (!m_selection) {
        throw std::runtime_error("No selection config set: call Analysis::setSelection() before Run()");
    }
    INFO("Applying selection from ", m_selection->sourcePath, "...");

    for (const auto& h : m_selection->histograms) bookHistogram(h);  // before any cut

    for (const auto& cut : m_selection->cuts) {
        std::string why;
        if (!requirementsMet(cut.requirements, why)) {
            INFO("Skipping cut: ", cut.name, " (", why, ")");
        } else {
            applyCut(cut.expression, cut.name, parseDataType(cut.dataType));
        }
        // Histograms attached to a cut are booked at this point of the selection even if the cut
        // itself is not applied to this sample; they have their own data_type / requires.
        for (const auto& h : cut.histograms) bookHistogram(h);
    }

    INFO("Booked ", m_histResults.size(), " histograms (", m_nHistSkipped, " skipped for this sample / reduced charge source; use -v for details).");
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

    // Backwards compatibility with older NTuples: VetoSt* naming, missing Veto11
    setupVetoCompatibility();

    // ── Built-in definitions ────────────────────────────────────────────────
    // These stay in C++ because the built-in reduced-charge code relies on them.
    // All other definitions are in config/definitions.yaml (applied at the end of this function).
    // Run periods
    Define("isCaloNuPeriod", "15821 <= run  && run <= 16924", DATA);
    Define("isCaloNuPeriod", "(200137 <= run && run <= 200147) || (200172 <= run && run <= 200183)", MC);
    Define("is2024Period", "run >= 1.2e4", DATA);
    Define("is2024Period", "(200091 < run && run < 200101) || (200160 <= run && run <= 200171) || (200137 <= run && run <= 200147) || (200172 <= run && run <= 200183)", MC);

    // Scintillator status flags (data only; used by the built-in GoodScintillatorStatus)
    if (!isMC) {
        Define("BadVetoStatus", "Veto20_status == 528 || Veto21_status == 528", DATA);
        Define("GoodVetoNuStatus", "((VetoNu0_status == 0 || VetoNu0_status == 1) && (VetoNu1_status == 0 || VetoNu1_status == 1)) || (VetoNu0_status == 0 && VetoNu1_status == 16)", DATA);
    }

    // ── VetoNu reduced charge ─────────────────────────────────────────────
    // Priority: 1) aux (waveform) NTuples, 2) branches already in the input NTuple, 3) stop with an error.
    // --no-reduced-charge skips all of this and the VetoNu veto uses the raw charge instead.
    // TODO: once definitions/cuts/histograms are configurable, only load the reduced charge
    //       if something actually depends on it.
    const bool hasNativeReducedCharge = isColumnDefined("VetoNu0_reduced_charge") && isColumnDefined("VetoNu1_reduced_charge");

    if (!useReducedCharge) {
        m_reducedChargeSource = ReducedChargeSource::None;
        INFO("Reduced charge disabled (--no-reduced-charge): VetoNu veto will use the raw charge.");
    } else if (m_auxChainSet) {
        m_reducedChargeSource = ReducedChargeSource::Aux;
        if (hasNativeReducedCharge) {
            WARNING("Input NTuple already contains VetoNu*_reduced_charge, but aux files were provided: the aux values take priority and will overwrite them.");
        }
        INFO("Reduced charge source: aux (waveform) NTuples.");
        defineReducedChargeFromAux();
    } else if (hasNativeReducedCharge) {
        m_reducedChargeSource = ReducedChargeSource::Native;
        INFO("Reduced charge source: branches in the input NTuple.");
    } else {
        ERROR("The selection uses the VetoNu reduced charge, but no aux (waveform) files were provided and the input NTuple has no VetoNu*_reduced_charge branches.");
        ERROR("Either add 'waveform_paths' for this run to the file config, or run with --no-reduced-charge to veto on the raw VetoNu charge.");
        throw std::runtime_error("Reduced charge not available");
    }


    defineGRLTimeColumns();  // GoodTimes / ExcludedTimes (data only)

    // ── Definitions from config/definitions.yaml ────────────────────────────
    // After all built-in columns, so they can use them (period flags, reduced charge, GoodTimes, ...)
    applyDefinitions();


    // This needs to go at the end of all the definitions, otherwise the aux columns won't be available for cuts
    m_eventIDNode = m_node;
}

// Load the reduced VetoNu charge from the aux (waveform) NTuples via an eventID lookup,
// and define the columns needed to decide whether to trust it (with raw-charge fallback)
void Analysis::defineReducedChargeFromAux() {
    // Build a lookup map from the aux chain manually
    // Key: {run, event}, Value: struct of aux quantities
    struct AuxData { float charge35_nu0; float charge35_nu1; };
    
    // auto auxMap = std::make_shared<std::unordered_map<Int_t, AuxData>>();
    auto auxMap = std::make_shared<std::unordered_map<Int_t, AuxData>>();

    Int_t  run, evt;
    float charge35_nu0, charge35_nu1;

    // Only read the four branches we need (GetEntry would otherwise decompress every branch)
    m_auxChain->SetBranchStatus("*", false);
    for (const char* b : {"run", "event", "myVetoNu0_rawCharge35", "myVetoNu1_rawCharge35"}) {
        m_auxChain->SetBranchStatus(b, true);
    }

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
    m_auxChain->ResetBranchAddresses(); // the addresses above point to local variables

    // Inject aux quantities as new columns via Define()
    // Capture auxMap by shared_ptr so it stays alive
    // Define a column which flags if an event had a good hit in either veto station or preshower - need a good hit to trust the reduced charge values from the aux file
    // Veto status 512 is what is recorded as a good hit on the second digitizer (CaloNu period specific)
    Define("GoodVeto20Hit",[](int status) { return !std::isnan(status) && (status & ~512) == 0; }, {"Veto20_status"});
    Define("GoodVeto21Hit", [](int status) { return !std::isnan(status) && (status & ~512) == 0; }, {"Veto21_status"});
    Define("GoodPreshower0Hit", [](int status) { return !std::isnan(status) && (status & ~512) == 0; }, {"Preshower0_status"}); 
    Define("GoodPreshower1Hit", [](int status) { return !std::isnan(status) && (status & ~512) == 0; }, {"Preshower1_status"});

    Define("TimingOK", "(GoodVeto20Hit || GoodVeto21Hit || GoodPreshower0Hit || GoodPreshower1Hit)");
    if (!isMC) Define("GoodScintillatorStatus", "TimingOK && GoodVetoNuStatus && !BadVetoStatus", DATA);

    // If the input also has native reduced-charge branches, Redefine them with the aux values
    auto defineOrRedefine = [this](const std::string& name, auto f, const ROOT::RDF::ColumnNames_t& cols) {
        if (isColumnDefined(name)) m_node = m_node->Redefine(name, f, cols);
        else                       Define(name, f, cols);
    };

    defineOrRedefine("VetoNu0_reduced_charge",
        [this, auxMap](Int_t run, Int_t eventID, bool TimingOK, bool is2024Period, float VetoNu0_raw_charge) -> float {
            
            Int_t key = getLookupKey(run, eventID);
            bool lookup_success = auxMap->count(key);
            auto reduced_charge = -99999.f;
            
            if (lookup_success) {

                auto it = auxMap->find(key);
                reduced_charge = it->second.charge35_nu0;
                
                // If there's no good hit in the veto stations or preshower, use the original charge instead of the reduced charge

                bool is2024PeriodCondition = is2024Period && reduced_charge == 0 && VetoNu0_raw_charge > 30;

                if (is2024PeriodCondition)
                {
                    return -1;
                }

                if (!TimingOK || std::isnan(reduced_charge) || is2024PeriodCondition){
                    // m_NVetoNu0_fallbacks++;
                    // return VetoNu0_raw_charge;
                    return -1;
                }
                
                return std::max(reduced_charge, 0.0f);
            }
            
            ERROR("Warning: Missing aux data for eventID ", eventID, " in run ", run, ". Setting reduced charge to -999.");
            m_NVetoNu0_missing_aux++;
            return reduced_charge;
            
        }, {"run", "eventID", "TimingOK", "is2024Period", "VetoNu0_raw_charge"});
    
    defineOrRedefine("VetoNu1_reduced_charge",
        [this, auxMap](Int_t run, Int_t eventID, bool TimingOK, bool is2024Period, float VetoNu1_raw_charge) -> float {

            Int_t key = getLookupKey(run, eventID);
            auto reduced_charge = -99999.f;
            
            bool lookup_success = auxMap->count(key);
            if (lookup_success) {
                
                auto it = auxMap->find(key);
                reduced_charge = it->second.charge35_nu1;

                // If there's no good hit in the veto stations or preshower, use the original charge instead of the reduced charge
                bool is2024PeriodCondition = is2024Period && reduced_charge == 0 && VetoNu1_raw_charge > 30;

                if (is2024PeriodCondition)
                {
                    return -1;
                }

                if (!TimingOK || std::isnan(reduced_charge) || is2024PeriodCondition){
                    // m_NVetoNu0_fallbacks++;
                    // return VetoNu0_raw_charge;
                    return -1;
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

    Define("validReducedVetoNu0Charge", "AuxLookupSuccess && VetoNu0_reduced_charge >= 0");
    Define("validReducedVetoNu1Charge", "AuxLookupSuccess && VetoNu1_reduced_charge >= 0");

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

}

namespace {
    // Veto11_<var> column that returns Veto10_<var> and counts how often it is evaluated.
    // Typed (compiled) Define, so it needs the exact column type.
    template <typename T>
    void defineCountedFallback(Analysis& analysis, const std::string& target, const std::string& source,
                               std::shared_ptr<std::atomic<ULong64_t>> counter) {
        analysis.Define(target,
            [counter](T value) { counter->fetch_add(1, std::memory_order_relaxed); return value; },
            {source});
    }
}

// Backwards compatibility with older NTuples:
//  1) Old NTuples name the veto scintillator branches VetoSt<N>_<var> instead of Veto<N>_<var>.
//     Every VetoSt<N>_<var> gets an alias Veto<N>_<var>, so the code can always use the new names.
//  2) Veto11 was not read out in 2022-2023 (no free digitiser channel).
//    If the input has no Veto11_* branches at all, every Veto11_<var> is defined
//     as the corresponding Veto10_<var>. Its use is counted, and reportVetoFallbacks() prints a
//     warning at the end of the job for every Veto11 column that was actually used.
void Analysis::setupVetoCompatibility() {
    // 1) VetoSt<N>_<var> -> Veto<N>_<var>
    const std::regex oldVetoName(R"(^VetoSt(\d+)_(.+)$)");
    std::vector<std::string> aliased;
    for (const auto& col : m_node->GetColumnNames()) {
        std::smatch m;
        if (!std::regex_match(col, m, oldVetoName)) continue;
        const std::string newName = "Veto" + m[1].str() + "_" + m[2].str();
        if (isColumnDefined(newName)) continue;
        m_node = m_node->Alias(newName, col);
        aliased.push_back(col + " -> " + newName);
    }
    if (!aliased.empty()) {
        INFO("Old NTuple naming: aliased ", aliased.size(), " VetoSt* branches to the Veto* names (e.g. ", aliased.front(), ").");
        for (const auto& a : aliased) DEBUG("  alias: ", a);
    }

    // 2) Veto11 -> Veto10 fallback if the input has no Veto11 branches
    const auto columns = m_node->GetColumnNames();  // includes the aliases above
    const bool hasVeto11 = std::any_of(columns.begin(), columns.end(),
                                       [](const std::string& c) { return c.rfind("Veto11_", 0) == 0; });
    if (hasVeto11) return;

    for (const auto& source : columns) {
        if (source.rfind("Veto10_", 0) != 0) continue;
        const std::string target = "Veto11_" + source.substr(7);
        const std::string type   = m_node->GetColumnType(source);
        auto counter = std::make_shared<std::atomic<ULong64_t>>(0);

        if      (type == "Float_t"  || type == "float")  defineCountedFallback<Float_t >(*this, target, source, counter);
        else if (type == "Double_t" || type == "double") defineCountedFallback<Double_t>(*this, target, source, counter);
        else if (type == "Int_t"    || type == "int")    defineCountedFallback<Int_t   >(*this, target, source, counter);
        else if (type == "UInt_t"   || type == "unsigned int") defineCountedFallback<UInt_t>(*this, target, source, counter);
        else if (type == "Bool_t"   || type == "bool")   defineCountedFallback<Bool_t  >(*this, target, source, counter);
        else {
            // Unexpected type: plain copy, use not counted
            m_node = m_node->Define(target, source);
            counter = nullptr;
        }
        m_columnFallbacks.push_back({target, source, counter});
    }

    if (!m_columnFallbacks.empty()) {
        INFO("No Veto11 branches in the input (Veto11 was not read out in 2022-2023): ", m_columnFallbacks.size(),
             " Veto11_* columns will fall back to the Veto10_* values if used. A warning is printed at the end if they are.");
    }
}

// Warn about every fallback column that was actually used in the event loop
void Analysis::reportVetoFallbacks() const {
    for (const auto& fb : m_columnFallbacks) {
        if (!fb.nUsed) {
            WARNING("'", fb.target, "' is not in the input NTuple and was defined as '", fb.source,
                    "' (use not counted for this column type).");
        } else if (fb.nUsed->load() > 0) {
            WARNING("'", fb.target, "' is not in the input NTuple (no Veto11 in 2022-2023): '", fb.source,
                    "' was used instead for ", fb.nUsed->load(), " events.");
        }
    }
}

// Columns written to the output nt tree, from the output columns config (all columns if no config set)
std::vector<std::string> Analysis::selectOutputColumns() {
    const auto allColumns = m_node->GetColumnNames();
    if (!m_outputColumnsConfig) {
        INFO("No output columns config set: saving all ", allColumns.size(), " columns to ", m_mainFileTreeName, ".");
        return allColumns;
    }
    // Fallback columns (e.g. Veto11 -> Veto10) are compatibility shims, not real branches or framework
    // variables: they are never included by the save_all_* switches, only if a keep entry asks for them
    std::vector<std::string> fallbackColumns;
    for (const auto& fb : m_columnFallbacks) fallbackColumns.push_back(fb.target);
    const auto columns = ConfigUtils::selectOutputColumns(allColumns, m_node->GetDefinedColumnNames(), *m_outputColumnsConfig, isMC, fallbackColumns);
    INFO("Saving ", columns.size(), " of ", allColumns.size(), " columns to ", m_mainFileTreeName,
         " (config: ", m_outputColumnsConfig->sourcePath, "). Use -v to list them.");
    for (const auto& c : columns) DEBUG("  saving column: ", c);
    return columns;
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

    if (!appliesTo(dataType)) {
        INFO("Skipping cut for ", (isAsimov ? "Asimov" : (isMC ? "MC" : "data")), ": ", cutName);
        return;
    }

    INFO("Applying cut: ", cutName, " (", cutExpression, ")");

    std::string pass_cut_name = "passed_" + cutName;
    const std::vector<std::pair<std::string, std::string>> replacements = {
        {"<=", "leq"}, {">=", "geq"}, {"==", "eq"}, {"=", "eq"},
        {"<",  "lt"},  {">",  "gt"},
        {" ",  "_"}, {"&&", "and"}, {"||", "or"}, {"&", "and"}, {"|", "or"}, {"!", "not"},
        {"(", ""}, {")", ""}, {".", "_"}, {"/", "_"}, {"\\", "_"}, {"[", "_"}, {"]", "_"}, 
        {"-", "minus"}, {"+", "plus"}, {"*", "times"}, {"%", "percent"}, {"^", "caret"}
    };

    for (const auto& [from, to] : replacements) {
        replaceAll(pass_cut_name, from, to);
    }
    
    if (std::find(m_passedCutColNames.begin(), m_passedCutColNames.end(), pass_cut_name) != m_passedCutColNames.end()) {
        throw std::runtime_error("Cut name '" + cutName + "' gives the eventID_pass flag '" + pass_cut_name +
                                 "', which is already used by another cut. Please rename one of the cuts.");
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

    // ── Cuts and histograms from the selection config ──────────────────
    applySelection();

    // ── Book ALL actions before triggering any event loop ──────────────────
    // TODO: Move to a Finalise() function
    auto cutReport = m_node->Report();
    // Run range in the input (before any cuts), only used as a sanity check against m_runNumbers.
    // Booked here so it is filled in the same event loop as everything else.
    auto inputRunMin = m_df->Min<int>("run");
    auto inputRunMax = m_df->Max<int>("run");

    if (outputFileName != "") {
        INFO("Saving snapshot to ", outputFileName, "...");

        // Both snapshots are booked lazily so that they are filled in ONE event loop together with
        // the cutflow, histograms and counters booked above.
        // RDataFrame cannot write two trees to the same file in one event loop, so the eventID_pass
        // tree is written to a temporary file next to the output and copied in afterwards
        // (a fast, basket-level copy: no re-reading of the input).
        auto opts = ROOT::RDF::RSnapshotOptions();
        opts.fMode = "RECREATE";
        opts.fLazy = true;
        const auto columns = selectOutputColumns();
        auto ntSnapshot = m_node->Snapshot(m_mainFileTreeName, outputFileName, columns, opts);

        const std::string eventIDTmpFile = std::string(outputFileName.Data()) + ".eventID_pass.tmp.root";
        m_passedCutColNames.push_back("run");
        m_passedCutColNames.push_back("eventID");
        auto eventIDSnapshot = m_eventIDNode->Snapshot("eventID_pass", eventIDTmpFile, m_passedCutColNames, opts);

        // ── THE event loop ──────────────────────────────────────────────────
        INFO("Running the event loop: writing ", columns.size(), " columns to ", m_mainFileTreeName, " and ",
             m_passedCutColNames.size(), " columns to eventID_pass...");
        ntSnapshot.GetValue();       // triggers the (single) event loop
        eventIDSnapshot.GetValue();  // already filled by the same loop

        // ── Post-processing to save metadata and cutflow info ───────────────
        TFile *file = TFile::Open(outputFileName, "UPDATE");

        // ── Metadata tree ────────────────────────────────────────────────────
        // Filled from the run numbers this job was asked to process (m_runNumbers), NOT from the
        // events that pass the cuts, so the lumi is correct even if no events are selected.
        TTree *meta_tree = new TTree("meta", "Metadata tree");

        ULong64_t nEventsInput = nEventsBeforeCuts.GetValue();
        if (m_runNumbers.empty()) {
            WARNING("No run numbers set (Analysis::setRunNumbers). The meta tree will have no runs.");
        }
        if (nEventsInput == 0) {
            WARNING("No events in the input files. The nt tree will be empty.");
        } else {
            // Sanity check: the input files should only contain the requested run(s)
            const std::set<int> requested(m_runNumbers.begin(), m_runNumbers.end());
            if (!requested.count(*inputRunMin) || !requested.count(*inputRunMax)) {
                WARNING("Input files contain run numbers in [", *inputRunMin, ", ", *inputRunMax,
                        "] which are not all in the requested run list. Check the file config!");
            }
        }

        std::vector<int>   runBranch(m_runNumbers.begin(), m_runNumbers.end());
        std::vector<float> lumiBranch;
        for (const auto& run : runBranch) {
            auto it = m_runLumiDict.find(run);
            lumiBranch.push_back(it != m_runLumiDict.end() ? it->second : -1.f);

            if (it == m_runLumiDict.end()) {
                if (isMC) INFO("MC run ", run, " has no GRL lumi. Setting lumi to -1.");
                else      WARNING("Run ", run, " not found in GRL! Setting lumi to -1.");
            }
        }

        float totalLumi = std::accumulate(lumiBranch.begin(), lumiBranch.end(), 0.f);
        INFO("Total integrated luminosity for this job: ", totalLumi, " /pb");

        meta_tree->Branch("run_number",     &runBranch);
        meta_tree->Branch("lumi",           &lumiBranch);
        meta_tree->Branch("n_events_input", &nEventsInput);
        meta_tree->Fill();
        meta_tree->Write();
        INFO("Saved metadata tree with ", runBranch.size(), " run(s).");

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

        // ── Histograms ────────────────────────────
        if (!m_histResults.empty()) {
            INFO("Writing histograms to file...");
            // make TDirectory for histograms
            TFile *histFile = TFile::Open(outputFileName, "UPDATE");
            histFile->mkdir("histograms");
            histFile->cd("histograms");
            for (auto&& hist : m_histResults) {
                hist->Write();
            }
            histFile->Close();
            INFO("Wrote ", m_histResults.size(), " histograms to file.");
        }

        // ── Event ID pass tree: copy from the temporary file into the output ─
        {
            std::unique_ptr<TFile> tmpFile(TFile::Open(eventIDTmpFile.c_str(), "READ"));
            std::unique_ptr<TFile> outFile(TFile::Open(outputFileName, "UPDATE"));
            if (!tmpFile || tmpFile->IsZombie() || !outFile || outFile->IsZombie()) {
                throw std::runtime_error("Could not open " + eventIDTmpFile + " or " + std::string(outputFileName.Data()) + " to copy the eventID_pass tree");
            }
            auto* eventIDTree = tmpFile->Get<TTree>("eventID_pass");
            if (!eventIDTree) {
                throw std::runtime_error("eventID_pass tree not found in " + eventIDTmpFile);
            }
            outFile->cd();
            TTree* copy = eventIDTree->CloneTree(-1, "fast");
            copy->Write();
            INFO("Wrote eventID_pass tree with ", copy->GetEntries(), " entries.");
            outFile->Close();
            tmpFile->Close();
        }
        std::filesystem::remove(eventIDTmpFile);

    } else {
        cutReport->Print();
    }

    INFO("Total event loop runs: ", m_node->GetNRuns());
    if (m_node->GetNRuns() > 1) {  // should be exactly 1
        WARNING("Event loop ran multiple times. This is inefficient.");
    }

    // Veto11 -> Veto10 fallback (only reported if used)
    reportVetoFallbacks();

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