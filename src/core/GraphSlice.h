#pragma once

#include "core/Types.h"
#include "core/Edge.h"
#include <vector>
#include <unordered_set>
#include <string>

namespace cg {

struct QueryParams {
    SymbolId        root         = INVALID_SYMBOL_ID;
    int             maxDepth     = 3;
    EdgeKindMask    edgeFilter   = ALL_EDGE_KINDS;
    ConfidenceMask  confFilter   = ALL_CONFIDENCE;
    bool            includeCallers = true;
    bool            includeCallees = true;
};

struct NodeRef {
    SymbolId id;
    int      depth        = 0;   // negative = caller side, positive = callee side
    bool     isBoundary   = false; // reached depth limit but has further neighbours
};

struct EdgeRef {
    EdgeId id;
};

struct GraphSlice {
    SymbolId            root;
    std::vector<NodeRef> nodes;
    std::vector<EdgeRef> edges;
    QueryParams          params;

    bool hasNode(SymbolId id) const {
        for (const auto& n : nodes)
            if (n.id == id) return true;
        return false;
    }
};

} // namespace cg
