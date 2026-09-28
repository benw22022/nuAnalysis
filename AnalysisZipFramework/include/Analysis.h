#include <TFile.h>
#include <TTree.h>
#include <TString.h>
#include <TChain.h>
#include <ROOT/RDataFrame.hxx>
#include <iostream>
#include <vector>
#include <stdio.h>
#include <optional>
#include <unordered_map>
#include <atomic>
#include "MessageService.hpp"
#include "GRLUtils.h"
#include "ConfigUtils.h"
#include <memory>
#include <stdexcept>

enum DataType { MC, DATA, ALL, ASIMOV };

class Analysis {
    public:
        Analysis();
        Analysis(TString mainFileTreeName, const std::vector<TString>& mainFiles);
        Analysis(TString mainFileTreeName, TString auxFileTreeName, const std::vector<TString>& mainFiles, const std::vector<TString>& auxFiles);

        Analysis(TString mainFileTreeName, TString mainFiles);
        Analysis(TString mainFileTreeName, TString auxFileTreeName, TString mainFiles, TString auxFiles);
        

        void addMainFiles(TString mainFileTreeName, std::vector<TString> mainFiles);
        void addAuxFiles(TString auxFileTreeName, std::vector<TString> auxFiles);
        
        void addMainFiles(TString mainFileTreeName, TString mainFile);
        void addAuxFiles(TString auxFileTreeName, TString auxFile);
        
        void BuildDataFrame();

        void Run(TString outputFileName = "");

        void setGRL(const std::vector<TString>& grlJsons, const std::vector<TString>& grlCSVs);

        bool isMC{false};

        bool isAsimov{false};

        // If false, the reduced VetoNu charge is not used at all: no aux files are loaded
        // and the VetoNu veto is applied on the raw charge instead (see Run()).
        bool useReducedCharge{true};

        // Run numbers this job was asked to process. Used to fill the meta tree (lumi),
        // independently of whether any events pass the cuts.
        void setRunNumbers(const std::vector<int>& runs) { m_runNumbers = runs; }

        // Which columns to write to the output nt tree (see config/output_columns.yaml).
        // If not set, all columns are written.
        void setOutputColumns(const ConfigUtils::OutputColumnsConfig& config) { m_outputColumnsConfig = config; }

        // Extra column definitions (see config/definitions.yaml). Required before Run().
        void setDefinitions(const ConfigUtils::DefinitionsConfig& definitions) { m_definitions = definitions; }

        // Cuts and histograms to apply (see config/cuts.yaml). Required before Run().
        void setSelection(const ConfigUtils::SelectionConfig& selection) { m_selection = selection; }

        // ── Adding columns ───────────────────────────────────────────────────
        // All new columns (built-in C++ and config/definitions.yaml) go through these methods, so
        // they all get the same data_type handling, name checks, logging and bookkeeping.
        //   dataType: same meaning as in the YAML configs (ALL, DATA, MC = MC but not Asimov,
        //             ASIMOV = all MC)
        //   origin:   where the definition comes from (e.g. "built-in", a config file path),
        //             used in error messages and in the definition log
        // Define returns false (and does nothing) if dataType does not apply to this sample.
        // It throws if a column with that name already exists (use Redefine to replace one).

        // JIT-compiled expression, e.g. Define("pz_GeV", "pz / 1000")
        bool Define(const std::string& name, const std::string& expression,
                    DataType dataType = ALL, const std::string& origin = "built-in");

        // Compiled C++ callable, e.g. Define("x", [](float a) { return 2 * a; }, {"a"})
        template <typename F>
        bool Define(const std::string& name, F function, const ROOT::RDF::ColumnNames_t& columns,
                    DataType dataType = ALL, const std::string& origin = "built-in") {
            if (!appliesTo(dataType)) {
                DEBUG("Skipping definition ", name, " (not for this sample)");
                return false;
            }
            checkNewColumnName(name, origin);
            m_node = m_node->Define(name, function, columns);
            recordDefinition("Define", name, "<compiled C++> of (" + joinColumns(columns) + ")", dataType, origin);
            return true;
        }

        // Replace an existing column (throws if it does not exist)
        template <typename F>
        void Redefine(const std::string& name, F function, const ROOT::RDF::ColumnNames_t& columns,
                      const std::string& origin = "built-in") {
            if (!isColumnDefined(name)) {
                throw std::runtime_error(origin + ": cannot redefine '" + name + "', no column with that name exists");
            }
            m_node = m_node->Redefine(name, function, columns);
            recordDefinition("Redefine", name, "<compiled C++> of (" + joinColumns(columns) + ")", ALL, origin);
        }

        // Does a column (input branch, definition or alias) with this name exist?
        bool isColumnDefined(const std::string& columnName);

        // New name for an existing column (throws if the alias name is already taken)
        void Alias(const std::string& alias, const std::string& column, const std::string& origin = "built-in");

        // Every column added through Define / Redefine / Alias, in order (for provenance / debugging)
        struct DefinitionRecord {
            std::string kind;        // Define, Redefine or Alias
            std::string name;
            std::string expression;  // expression, "<compiled C++> of (inputs)" or the aliased column
            std::string dataType;
            std::string origin;
        };
        const std::vector<DefinitionRecord>& definitionLog() const { return m_definitionLog; }
    
    private:
        TString m_mainFileTreeName;
        TString m_auxFileTreeName;

        bool m_mainChainSet{false};
        bool m_auxChainSet{false};

        std::vector<TString> m_mainFileNames{};
        std::vector<TString> m_auxFileNames{};

        TChain *m_mainChain = nullptr;
        TChain *m_auxChain = nullptr;

        std::unique_ptr<ROOT::RDataFrame> m_df;
        std::optional<ROOT::RDF::RNode> m_node;
        std::optional<ROOT::RDF::RNode> m_eventIDNode;

        std::vector<std::string> m_passedCutColNames;

        std::vector<int> m_runNumbers;

        std::optional<ConfigUtils::OutputColumnsConfig> m_outputColumnsConfig;
        std::vector<std::string> selectOutputColumns();

        // Where the VetoNu reduced charge comes from (decided in BuildDataFrame)
        enum class ReducedChargeSource { None, Aux, Native };
        ReducedChargeSource m_reducedChargeSource{ReducedChargeSource::None};
        void defineReducedChargeFromAux();

        std::vector<TString> ExpandAndSort(TString pattern);

        std::unordered_map<int, float> m_runLumiDict;
        // Good / excluded time ranges per run from the GRL (used by the GoodTimes / ExcludedTimes columns)
        std::shared_ptr<const GRLUtils::GRLTimes> m_grlTimes;
        void defineGRLTimeColumns();

        // Backwards compatibility with older NTuples (see setupVetoCompatibility in Analyis.cxx)
        void setupVetoCompatibility();
        void reportVetoFallbacks() const;
        struct ColumnFallback {
            std::string target;   // requested column, e.g. Veto11_charge
            std::string source;   // column used instead, e.g. Veto10_charge
            std::shared_ptr<std::atomic<ULong64_t>> nUsed;  // events for which the fallback was evaluated (nullptr: not counted)
        };
        std::vector<ColumnFallback> m_columnFallbacks;

        std::atomic<int> m_NVetoNu0_fallbacks{0};
        std::atomic<int> m_NVetoNu1_fallbacks{0};
        std::atomic<int> m_NVetoNu0_missing_aux{0};
        std::atomic<int> m_NVetoNu1_missing_aux{0};

        Int_t getLookupKey(int run, Int_t event) {
            return event;
            // return (uint64_t)run << 32 | (uint32_t)event;
        }

        void applyCut(std::string cutExpression, std::string cutName, DataType dataType = ALL);

        // Bookkeeping for Define / Redefine / Alias
        std::vector<DefinitionRecord> m_definitionLog;
        void checkNewColumnName(const std::string& name, const std::string& origin);
        void recordDefinition(const std::string& kind, const std::string& name, const std::string& expression,
                              DataType dataType, const std::string& origin);
        static std::string joinColumns(const ROOT::RDF::ColumnNames_t& columns);
        static std::string dataTypeName(DataType dataType);

        // Column definitions from the definitions config (config/definitions.yaml)
        std::optional<ConfigUtils::DefinitionsConfig> m_definitions;
        void applyDefinitions();

        // Cuts and histograms from the selection config (config/cuts.yaml)
        std::optional<ConfigUtils::SelectionConfig> m_selection;
        void applySelection();
        void bookHistogram(const ConfigUtils::HistogramConfig& cfg);
        std::size_t m_nHistSkipped{0};

        // Does something with this data_type apply to the current sample (data / MC / Asimov)?
        bool appliesTo(DataType dataType) const;
        static DataType parseDataType(const std::string& dataType);
        // Are all 'requires' conditions met? If not, `why` explains which one is not.
        bool requirementsMet(const std::vector<std::string>& requirements, std::string& why) const;

        std::vector<ROOT::RDF::RResultPtr<TH1>> m_histResults;

};
