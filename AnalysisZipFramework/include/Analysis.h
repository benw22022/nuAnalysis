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

enum DataType { MC, DATA, ALL};

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

        void Define(std::string columnName, std::string expression, DataType dataType = ALL);
    
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

        std::vector<TString> ExpandAndSort(TString pattern);

        std::unordered_map<int, float> m_runLumiDict;
        std::string m_excludedTimesCut{""};
        std::string m_goodTimesCut{""};

        std::atomic<int> m_NVetoNu0_fallbacks{0};
        std::atomic<int> m_NVetoNu1_fallbacks{0};
        std::atomic<int> m_NVetoNu0_missing_aux{0};
        std::atomic<int> m_NVetoNu1_missing_aux{0};

        Int_t getLookupKey(int run, Int_t event) {
            return event;
            // return (uint64_t)run << 32 | (uint32_t)event;
        }

        void applyCut(std::string cutExpression, std::string cutName, DataType dataType = ALL);
};
