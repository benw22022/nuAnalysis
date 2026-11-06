#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// InputUtils: finding input files without the file config (--input-dir / --aux-dir)
// ─────────────────────────────────────────────────────────────────────────────
#include <string>
#include <vector>

namespace InputUtils {

    // All regular files ending in ".root" directly in `dir` (not in subdirectories), sorted.
    // Throws if `dir` does not exist, is not a directory, or contains no .root files.
    std::vector<std::string> listRootFiles(const std::string& dir);

    // Run numbers in the files, read from the first entry of `treeName` in each file
    // (FASER NTuple files contain a single run). Sorted, without duplicates.
    // Files whose tree is empty are skipped. Throws if a file or the 'run' branch cannot be read.
    std::vector<int> runNumbersInFiles(const std::vector<std::string>& files, const std::string& treeName);

} // namespace InputUtils
