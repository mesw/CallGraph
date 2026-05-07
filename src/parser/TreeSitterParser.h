#pragma once

#include <string>
#include <string_view>

// Forward-declare tree-sitter types to avoid including the C header in .h files
struct TSParser;
struct TSTree;
struct TSNode;
struct TSLanguage;

namespace cg {

// Thin RAII wrapper around a TSParser* and TSTree*.
// Each parser thread creates its own TreeSitterParser instance.
class TreeSitterParser {
public:
    TreeSitterParser();
    ~TreeSitterParser();

    TreeSitterParser(const TreeSitterParser&) = delete;
    TreeSitterParser& operator=(const TreeSitterParser&) = delete;

    // Parse the given source text. Returns true on success (even partial).
    // The source string must remain valid for the lifetime of the tree.
    bool parse(const char* source, std::size_t length);

    TSTree* tree() const { return m_tree; }

    // True if the most recent parse contained any ERROR nodes
    bool hasErrors() const { return m_hasErrors; }

private:
    TSParser* m_parser = nullptr;
    TSTree*   m_tree   = nullptr;
    bool      m_hasErrors = false;

    void checkErrors();
};

} // namespace cg
