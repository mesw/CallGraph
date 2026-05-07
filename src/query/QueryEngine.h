#pragma once

#include "core/Index.h"
#include "core/GraphSlice.h"
#include <string_view>
#include <vector>

namespace cg {

// BFS callers (reverse direction) from root up to max_depth levels.
GraphSlice callersOf(const Index& index, SymbolId root,
                     int maxDepth,
                     EdgeKindMask edgeFilter   = ALL_EDGE_KINDS,
                     ConfidenceMask confFilter  = ALL_CONFIDENCE);

// BFS callees (forward direction) from root up to max_depth levels.
GraphSlice calleesOf(const Index& index, SymbolId root,
                     int maxDepth,
                     EdgeKindMask edgeFilter   = ALL_EDGE_KINDS,
                     ConfidenceMask confFilter  = ALL_CONFIDENCE);

// Union of callers and callees centred on root.
GraphSlice neighboursOf(const Index& index, SymbolId root,
                        int maxDepth,
                        EdgeKindMask edgeFilter  = ALL_EDGE_KINDS,
                        ConfidenceMask confFilter = ALL_CONFIDENCE);

// Name search
std::vector<SymbolId> findByName(const Index& index,
                                 std::string_view query,
                                 SearchMode mode);

} // namespace cg
