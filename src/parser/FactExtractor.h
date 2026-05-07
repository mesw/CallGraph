#pragma once

#include "parser/Facts.h"
#include <atomic>
#include <filesystem>
#include <string>

struct TSTree;
struct TSNode;

namespace cg {

// Extracts all fact records from a single parsed file.
// Not thread-safe on its own; each parser thread creates its own instance.
class FactExtractor {
public:
    explicit FactExtractor(FactBuffer& buffer,
                           std::atomic<uint64_t>& nextSymbolId);

    void extractFromFile(const std::filesystem::path& path,
                         const char* source,
                         std::size_t sourceLen,
                         TSTree* tree);

private:
    FactBuffer&             m_buffer;
    std::atomic<uint64_t>&  m_nextSymbolId;
    std::string             m_currentFile;
    const char*             m_source = nullptr;

    // Per-file state
    std::string             m_currentNamespace;
    std::string             m_currentClass;
    SymbolId                m_currentFunctionId = INVALID_SYMBOL_ID;
    bool                    m_insideMacroExpansion = false;

    SymbolId newId();

    // Node text helpers
    std::string nodeText(TSNode node) const;
    std::string nodeFieldText(TSNode node, const char* field) const;

    // Recursive visitor
    void visitNode(TSNode node, int depth);

    // Specific extractors
    void extractFunctionDefinition(TSNode node);
    void extractCallExpression(TSNode node);
    void extractAssignmentExpression(TSNode node);
    void extractPreprocDef(TSNode node);
    void extractPreprocFunctionDef(TSNode node);
    void extractClassSpecifier(TSNode node);

    // Helpers
    std::string buildQualifiedName(const std::string& name) const;
    bool isThreadSpawnArg(TSNode callNode, TSNode argNode) const;
    std::string extractCalleeText(TSNode calleeNode) const;
    int countArguments(TSNode argumentList) const;
    void extractBodyCalls(const std::string& body,
                          std::vector<std::string>& calls) const;
};

} // namespace cg
