#pragma once

#include "core/Index.h"
#include "parser/Facts.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace cg {

struct ResolverOptions {
    bool enableVirtualExpansion = false;
};

// Consumes the FactBuffer and produces a fully-built, immutable Index.
class Resolver {
public:
    explicit Resolver(ResolverOptions opts = {});

    // Runs all resolution steps and returns the completed Index.
    // After this call, the FactBuffer is no longer needed.
    std::unique_ptr<Index> resolve(FactBuffer& facts);

private:
    ResolverOptions m_opts;

    // Symbol table: qualified_name → [SymbolId, ...]
    std::unordered_map<std::string, std::vector<SymbolId>> m_symbolTable;

    // Class hierarchy: class_name → [base_names]
    std::unordered_map<std::string, std::vector<std::string>> m_classHierarchy;

    // Transitive subclasses: base_name → [all subclass names]
    std::unordered_map<std::string, std::vector<std::string>> m_subclasses;

    // The index being built
    Index* m_index = nullptr; // non-owning, owned by caller

    // Step implementations
    void buildSymbolTable(const std::vector<FunctionDef>& defs);
    void buildClassHierarchy(const std::vector<ClassDecl>& decls);
    void resolveCall(const CallSite& cs);
    void resolveFuncPtrAssign(const FuncPtrAssign& assign);
    void expandVirtuals();
    void resolveMacroEdges(const std::vector<MacroDef>& macros);

    // Helpers
    std::vector<SymbolId> lookupByNameArity(const std::string& name, int arity) const;
    std::string stripScope(const std::string& qualName) const;
    SymbolId macroNodeId(const std::string& macroName);
    std::unordered_map<std::string, SymbolId> m_macroNodes;
    SymbolId m_nextMacroId = 1ULL << 61; // synthetic range for macros
};

} // namespace cg
