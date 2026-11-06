#include "InputUtils.h"
#include "MessageService.hpp"

#include <TFile.h>
#include <TTree.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <set>
#include <stdexcept>

namespace InputUtils {

    std::vector<std::string> listRootFiles(const std::string& dir) {
        namespace fs = std::filesystem;
        std::error_code ec;
        if (!fs::exists(dir, ec)) throw std::runtime_error("Input directory not found: " + dir);
        if (!fs::is_directory(dir, ec)) throw std::runtime_error("Not a directory: " + dir);

        std::vector<std::string> files;
        for (const auto& entry : fs::directory_iterator(dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".root") {
                files.push_back(entry.path().string());
            }
        }
        if (files.empty()) throw std::runtime_error("No .root files found in " + dir);
        std::sort(files.begin(), files.end());
        return files;
    }

    std::vector<int> runNumbersInFiles(const std::vector<std::string>& files, const std::string& treeName) {
        std::set<int> runs;
        for (const auto& path : files) {
            std::unique_ptr<TFile> f(TFile::Open(path.c_str(), "READ"));
            if (!f || f->IsZombie()) throw std::runtime_error("Could not open input file " + path);
            auto* tree = f->Get<TTree>(treeName.c_str());
            if (!tree) throw std::runtime_error("No tree '" + treeName + "' in " + path);
            if (tree->GetEntries() == 0) {
                WARNING("Input file ", path, " has no entries in tree '", treeName, "'.");
                continue;
            }
            Int_t run = -1;
            tree->SetBranchStatus("*", false);
            tree->SetBranchStatus("run", true);
            if (tree->SetBranchAddress("run", &run) < 0) {
                throw std::runtime_error("Could not read the 'run' branch (Int_t) of tree '" + treeName + "' in " + path);
            }
            tree->GetEntry(0);
            tree->ResetBranchAddresses();
            runs.insert(run);
        }
        return {runs.begin(), runs.end()};
    }

} // namespace InputUtils
