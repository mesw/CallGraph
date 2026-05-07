#pragma once

#include "core/Types.h"
#include "core/Edge.h"
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <optional>

namespace cg {

struct FunctionInfo {
    SymbolId    id;
    std::string qualified_name;
    std::string unqualified_name;
    int         arity         = 0;
    std::string class_qname;
    bool        is_virtual    = false;
    bool        is_static     = false;
    bool        is_definition = false;
    SourceLocation loc;
};

// Built once by the Resolver, then immutable for the rest of the session.
class Index {
public:
    void addSymbol(FunctionInfo info);
    void addEdge(Edge edge);
    void sortNames();   // call after all symbols are added, before queries

    // Symbol lookup
    const FunctionInfo* symbol(SymbolId id) const;
    std::vector<SymbolId> byName(std::string_view qualified_name) const;

    // Search
    std::vector<SymbolId> findBySubstring(std::string_view query) const;
    std::vector<SymbolId> findByRegex(std::string_view pattern) const;
    std::vector<SymbolId> findByExact(std::string_view qualified_name) const;

    // Adjacency
    const std::vector<EdgeId>& forwardEdges(SymbolId id) const;
    const std::vector<EdgeId>& reverseEdges(SymbolId id) const;
    const Edge* edge(EdgeId id) const;
    const std::vector<Edge>& allEdges() const { return m_edges; }
    const std::vector<std::string>& allNames() const { return m_allNames; }

    std::size_t symbolCount() const { return m_symbols.size(); }
    std::size_t edgeCount()   const { return m_edges.size(); }

    // Synthetic "Unresolved" node management
    SymbolId syntheticUnresolved(const std::string& callee_name);
    bool isSynthetic(SymbolId id) const;

private:
    static constexpr SymbolId SYNTHETIC_BASE = 1ULL << 62;

    std::unordered_map<SymbolId, FunctionInfo>              m_symbols;
    std::unordered_map<std::string, std::vector<SymbolId>>  m_byName;
    std::vector<Edge>                                        m_edges;
    std::unordered_map<SymbolId, std::vector<EdgeId>>        m_forward;
    std::unordered_map<SymbolId, std::vector<EdgeId>>        m_reverse;
    std::vector<std::string>                                 m_allNames; // sorted, for binary search

    std::unordered_map<std::string, SymbolId>                m_syntheticMap;
    SymbolId m_nextSyntheticId = SYNTHETIC_BASE + 1;

    static const std::vector<EdgeId> s_emptyEdgeList;
};

} // namespace cg
