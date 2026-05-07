#pragma once

#include "parser/Facts.h"
#include <atomic>
#include <filesystem>
#include <functional>
#include <vector>

namespace cg {

struct ParserPoolProgress {
    int filesParsed  = 0;
    int totalFiles   = 0;
    int parseErrors  = 0;
};

using ParserProgressFn = std::function<void(const ParserPoolProgress&)>;

// Runs tree-sitter parsing across the given file list using QThreadPool.
// Appends facts to the shared FactBuffer.
// Reports progress on a 100 ms timer.
// Blocks until all files are parsed.
void parseFiles(const std::vector<std::filesystem::path>& files,
                FactBuffer& buffer,
                std::atomic<uint64_t>& nextSymbolId,
                ParserProgressFn progress = nullptr);

} // namespace cg
