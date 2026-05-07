#include "resolver/Resolver.h"

#include <algorithm>
#include <queue>
#include <unordered_set>

namespace cg {

Resolver::Resolver(ResolverOptions opts)
    : m_opts(std::move(opts))
{}

std::unique_ptr<Index> Resolver::resolve(FactBuffer& facts) {
    auto index = std::make_unique<Index>();
    m_index = index.get();

    buildSymbolTable(facts.functionDefs);
    buildClassHierarchy(facts.classDecls);

    for (const auto& cs : facts.callSites)
        resolveCall(cs);

    for (const auto& assign : facts.funcPtrAssigns)
        resolveFuncPtrAssign(assign);

    if (m_opts.enableVirtualExpansion)
        expandVirtuals();

    resolveMacroEdges(facts.macroDefs);

    index->sortNames();
    m_index = nullptr;
    return index;
}

// ---------------------------------------------------------------------------
// Step 1: Symbol table
// ---------------------------------------------------------------------------

void Resolver::buildSymbolTable(const std::vector<FunctionDef>& defs) {
    for (const auto& def : defs) {
        if (!def.is_definition) continue;

        FunctionInfo info;
        info.id               = def.id;
        info.qualified_name   = def.qualified_name;
        info.unqualified_name = def.unqualified_name;
        info.arity            = def.arity;
        info.class_qname      = def.class_qname;
        info.is_virtual       = def.is_virtual;
        info.is_static        = def.is_static;
        info.is_definition    = true;
        info.loc              = def.loc;
        m_index->addSymbol(info);

        m_symbolTable[def.qualified_name].push_back(def.id);
        // Also index by unqualified name for fallback lookup
        if (def.unqualified_name != def.qualified_name)
            m_symbolTable[def.unqualified_name].push_back(def.id);
    }
}

// ---------------------------------------------------------------------------
// Step 2: Class hierarchy
// ---------------------------------------------------------------------------

void Resolver::buildClassHierarchy(const std::vector<ClassDecl>& decls) {
    for (const auto& decl : decls) {
        m_classHierarchy[decl.qualified_name] = decl.base_classes;
    }

    // Compute subclass → all subclasses (transitive)
    // BFS from each base class
    for (const auto& [cls, bases] : m_classHierarchy) {
        for (const auto& base : bases) {
            m_subclasses[base].push_back(cls);
        }
    }
}

// ---------------------------------------------------------------------------
// Step 3: Resolve calls
// ---------------------------------------------------------------------------

std::vector<SymbolId> Resolver::lookupByNameArity(const std::string& name,
                                                   int arity) const {
    // Try exact name first
    auto it = m_symbolTable.find(name);
    if (it != m_symbolTable.end()) {
        std::vector<SymbolId> candidates;
        for (SymbolId sid : it->second) {
            const FunctionInfo* sym = m_index->symbol(sid);
            if (sym && (sym->arity == arity || arity < 0)) {
                candidates.push_back(sid);
            }
        }
        if (!candidates.empty()) return candidates;
        // If no arity match, return all (arity mismatch might be variadic)
        return it->second;
    }

    // Try unqualified name
    auto pos = name.rfind("::");
    if (pos != std::string::npos) {
        std::string unqualified = name.substr(pos + 2);
        auto it2 = m_symbolTable.find(unqualified);
        if (it2 != m_symbolTable.end()) {
            return it2->second;
        }
    }

    return {};
}

void Resolver::resolveCall(const CallSite& cs) {
    if (cs.enclosing_function == INVALID_SYMBOL_ID) return;

    auto candidates = lookupByNameArity(cs.callee_name, cs.callee_arity);

    if (candidates.empty()) {
        // Unresolved — synthetic node
        SymbolId synth = m_index->syntheticUnresolved(cs.callee_name);
        Edge edge;
        edge.from       = cs.enclosing_function;
        edge.to         = synth;
        edge.kind       = EdgeKind::Unresolved;
        edge.confidence = Confidence::Unknown;
        edge.loc        = cs.loc;
        if (cs.inside_macro_expansion && !cs.macro_name.empty())
            edge.via_macro = cs.macro_name;
        m_index->addEdge(std::move(edge));
        return;
    }

    Confidence conf = (candidates.size() == 1)
        ? Confidence::Exact : Confidence::Overload;

    EdgeKind kind = cs.kind;
    if (cs.inside_macro_expansion) kind = EdgeKind::ViaMacro;

    for (SymbolId sid : candidates) {
        Edge edge;
        edge.from       = cs.enclosing_function;
        edge.to         = sid;
        edge.kind       = kind;
        edge.confidence = conf;
        edge.loc        = cs.loc;
        if (cs.inside_macro_expansion && !cs.macro_name.empty())
            edge.via_macro = cs.macro_name;
        m_index->addEdge(std::move(edge));
    }
}

// ---------------------------------------------------------------------------
// Step 4: Function pointer assignments
// ---------------------------------------------------------------------------

void Resolver::resolveFuncPtrAssign(const FuncPtrAssign& assign) {
    if (assign.enclosing_function == INVALID_SYMBOL_ID) return;

    auto candidates = lookupByNameArity(assign.source_function, -1);

    EdgeKind kind = assign.is_thread_spawn
        ? EdgeKind::PassedToThread : EdgeKind::StoredAsPointer;

    if (candidates.empty()) {
        SymbolId synth = m_index->syntheticUnresolved(assign.source_function);
        Edge edge;
        edge.from       = assign.enclosing_function;
        edge.to         = synth;
        edge.kind       = kind;
        edge.confidence = Confidence::Unknown;
        edge.loc        = assign.loc;
        m_index->addEdge(std::move(edge));
        return;
    }

    Confidence conf = (candidates.size() == 1)
        ? Confidence::Exact : Confidence::Overload;

    for (SymbolId sid : candidates) {
        Edge edge;
        edge.from       = assign.enclosing_function;
        edge.to         = sid;
        edge.kind       = kind;
        edge.confidence = conf;
        edge.loc        = assign.loc;
        m_index->addEdge(std::move(edge));
    }
}

// ---------------------------------------------------------------------------
// Step 5: Virtual override expansion (optional)
// ---------------------------------------------------------------------------

void Resolver::expandVirtuals() {
    // Collect all edges that are resolved calls to virtual methods
    // and expand them to include VirtualCandidate edges to overrides.
    const auto& edges = m_index->allEdges();
    std::vector<Edge> newEdges;

    for (const auto& edge : edges) {
        if (edge.kind != EdgeKind::MethodCall &&
            edge.kind != EdgeKind::DirectCall) continue;

        const FunctionInfo* sym = m_index->symbol(edge.to);
        if (!sym || !sym->is_virtual || sym->class_qname.empty()) continue;

        // Find all subclasses
        auto subIt = m_subclasses.find(sym->class_qname);
        if (subIt == m_subclasses.end()) continue;

        for (const auto& subclass : subIt->second) {
            // Look for same method name + arity in subclass
            std::string overrideName = subclass + "::" + sym->unqualified_name;
            auto candidates = lookupByNameArity(overrideName, sym->arity);
            for (SymbolId sid : candidates) {
                if (sid == edge.to) continue;
                Edge vEdge;
                vEdge.from       = edge.from;
                vEdge.to         = sid;
                vEdge.kind       = EdgeKind::VirtualCandidate;
                vEdge.confidence = Confidence::Heuristic;
                vEdge.loc        = edge.loc;
                newEdges.push_back(std::move(vEdge));
            }
        }
    }

    for (auto& e : newEdges)
        m_index->addEdge(std::move(e));
}

// ---------------------------------------------------------------------------
// Step 6: Macro edges
// ---------------------------------------------------------------------------

SymbolId Resolver::macroNodeId(const std::string& macroName) {
    auto it = m_macroNodes.find(macroName);
    if (it != m_macroNodes.end()) return it->second;
    SymbolId sid = m_nextMacroId++;
    m_macroNodes[macroName] = sid;

    FunctionInfo info;
    info.id               = sid;
    info.qualified_name   = "__macro__" + macroName;
    info.unqualified_name = macroName;
    info.is_definition    = false;
    m_index->addSymbol(info);
    return sid;
}

void Resolver::resolveMacroEdges(const std::vector<MacroDef>& macros) {
    for (const auto& macro : macros) {
        SymbolId macroSid = macroNodeId(macro.name);

        for (const auto& callName : macro.body_calls) {
            auto candidates = lookupByNameArity(callName, -1);
            if (candidates.empty()) {
                SymbolId synth = m_index->syntheticUnresolved(callName);
                Edge edge;
                edge.from       = macroSid;
                edge.to         = synth;
                edge.kind       = EdgeKind::ViaMacro;
                edge.confidence = Confidence::Unknown;
                edge.loc        = macro.loc;
                edge.via_macro  = macro.name;
                m_index->addEdge(std::move(edge));
            } else {
                Confidence conf = (candidates.size() == 1)
                    ? Confidence::Exact : Confidence::Overload;
                for (SymbolId sid : candidates) {
                    Edge edge;
                    edge.from       = macroSid;
                    edge.to         = sid;
                    edge.kind       = EdgeKind::ViaMacro;
                    edge.confidence = conf;
                    edge.loc        = macro.loc;
                    edge.via_macro  = macro.name;
                    m_index->addEdge(std::move(edge));
                }
            }
        }
    }
}

} // namespace cg
