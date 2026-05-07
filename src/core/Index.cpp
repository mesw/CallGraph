#include "core/Index.h"

#include <algorithm>
#include <regex>

namespace cg {

const std::vector<EdgeId> Index::s_emptyEdgeList;

void Index::addSymbol(FunctionInfo info) {
    SymbolId id = info.id;
    m_byName[info.qualified_name].push_back(id);
    m_symbols.emplace(id, std::move(info));
}

void Index::addEdge(Edge edge) {
    EdgeId eid = static_cast<EdgeId>(m_edges.size() + 1);
    edge.id = eid;
    m_forward[edge.from].push_back(eid);
    m_reverse[edge.to].push_back(eid);
    m_edges.push_back(std::move(edge));
}

void Index::sortNames() {
    m_allNames.clear();
    m_allNames.reserve(m_byName.size());
    for (const auto& [name, _] : m_byName) {
        m_allNames.push_back(name);
    }
    std::sort(m_allNames.begin(), m_allNames.end());
}

const FunctionInfo* Index::symbol(SymbolId id) const {
    auto it = m_symbols.find(id);
    return it != m_symbols.end() ? &it->second : nullptr;
}

std::vector<SymbolId> Index::byName(std::string_view qualified_name) const {
    auto it = m_byName.find(std::string(qualified_name));
    if (it == m_byName.end()) return {};
    return it->second;
}

std::vector<SymbolId> Index::findBySubstring(std::string_view query) const {
    std::vector<SymbolId> result;
    std::string q(query);
    for (const auto& name : m_allNames) {
        if (name.find(q) != std::string::npos) {
            for (SymbolId sid : m_byName.at(name))
                result.push_back(sid);
        }
    }
    return result;
}

std::vector<SymbolId> Index::findByRegex(std::string_view pattern) const {
    std::vector<SymbolId> result;
    try {
        std::regex re(std::string(pattern), std::regex::ECMAScript | std::regex::optimize);
        for (const auto& name : m_allNames) {
            if (std::regex_search(name, re)) {
                for (SymbolId sid : m_byName.at(name))
                    result.push_back(sid);
            }
        }
    } catch (const std::regex_error&) {
        // invalid pattern — return empty
    }
    return result;
}

std::vector<SymbolId> Index::findByExact(std::string_view qualified_name) const {
    return byName(qualified_name);
}

const std::vector<EdgeId>& Index::forwardEdges(SymbolId id) const {
    auto it = m_forward.find(id);
    return it != m_forward.end() ? it->second : s_emptyEdgeList;
}

const std::vector<EdgeId>& Index::reverseEdges(SymbolId id) const {
    auto it = m_reverse.find(id);
    return it != m_reverse.end() ? it->second : s_emptyEdgeList;
}

const Edge* Index::edge(EdgeId id) const {
    if (id == INVALID_EDGE_ID || id > m_edges.size()) return nullptr;
    return &m_edges[id - 1];
}

SymbolId Index::syntheticUnresolved(const std::string& callee_name) {
    auto it = m_syntheticMap.find(callee_name);
    if (it != m_syntheticMap.end()) return it->second;

    SymbolId sid = m_nextSyntheticId++;
    m_syntheticMap[callee_name] = sid;

    FunctionInfo info;
    info.id             = sid;
    info.qualified_name = callee_name;
    info.unqualified_name = callee_name;
    info.is_definition  = false;
    m_symbols.emplace(sid, std::move(info));
    return sid;
}

bool Index::isSynthetic(SymbolId id) const {
    return id >= SYNTHETIC_BASE;
}

} // namespace cg
