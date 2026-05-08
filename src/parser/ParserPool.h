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
    // Hard errors: file could not be opened or tree-sitter returned no tree.
    // These files are skipped entirely and added to FactBuffer::parseErrors.
    int hardErrors   = 0;
    // Soft errors: tree-sitter parsed the file but flagged ERROR nodes.
    // Extraction still runs on the partial tree — results are approximate.
    int softErrors   = 0;
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
