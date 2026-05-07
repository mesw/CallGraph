#include "query/QueryEngine.h"

#include <queue>
#include <unordered_map>
#include <unordered_set>

namespace cg {

static bool edgePassesFilter(const Edge& e,
                              EdgeKindMask edgeFilter,
                              ConfidenceMask confFilter) {
    if (!(edgeFilter & edgeBit(e.kind))) return false;
    if (!(confFilter & confidenceBit(e.confidence))) return false;
    return true;
}

// ---------------------------------------------------------------------------
// Generic BFS helper
// ---------------------------------------------------------------------------

// direction: forward=callee traversal, reverse=caller traversal
static GraphSlice bfs(const Index& index,
                       SymbolId root,
                       int maxDepth,
                       EdgeKindMask edgeFilter,
                       ConfidenceMask confFilter,
                       bool forward,
                       bool includeCallers,
                       bool includeCallees) {
    GraphSlice slice;
    slice.root = root;
    slice.params.root            = root;
    slice.params.maxDepth        = maxDepth;
    slice.params.edgeFilter      = edgeFilter;
    slice.params.confFilter      = confFilter;
    slice.params.includeCallers  = includeCallers;
    slice.params.includeCallees  = includeCallees;

    if (index.symbol(root) == nullptr) return slice;

    // depth map: nodeId → depth (negative = caller side, positive = callee side)
    std::unordered_map<SymbolId, int> depthMap;
    std::unordered_set<EdgeId> includedEdges;

    struct QEntry { SymbolId id; int depth; };
    std::queue<QEntry> queue;
    queue.push({root, 0});
    depthMap[root] = 0;

    while (!queue.empty()) {
        auto [id, depth] = queue.front();
        queue.pop();

        int sign = forward ? 1 : -1;
        if (std::abs(depth) >= maxDepth) continue;

        const auto& adjacency = forward
            ? index.forwardEdges(id)
            : index.reverseEdges(id);

        for (EdgeId eid : adjacency) {
            const Edge* e = index.edge(eid);
            if (!e) continue;
            if (!edgePassesFilter(*e, edgeFilter, confFilter)) continue;

            SymbolId neighbor = forward ? e->to : e->from;
            includedEdges.insert(eid);

            if (depthMap.find(neighbor) == depthMap.end()) {
                int newDepth = depth + sign;
                depthMap[neighbor] = newDepth;
                queue.push({neighbor, newDepth});
            }
        }
    }

    // Determine boundary nodes
    std::unordered_set<SymbolId> boundaryNodes;
    for (const auto& [id, depth] : depthMap) {
        if (std::abs(depth) == maxDepth) {
            // Check if this node has further neighbours in the BFS direction
            const auto& adj = forward
                ? index.forwardEdges(id)
                : index.reverseEdges(id);
            for (EdgeId eid : adj) {
                const Edge* e = index.edge(eid);
                if (!e) continue;
                if (!edgePassesFilter(*e, edgeFilter, confFilter)) continue;
                SymbolId nb = forward ? e->to : e->from;
                if (depthMap.find(nb) == depthMap.end()) {
                    boundaryNodes.insert(id);
                    break;
                }
            }
        }
    }

    for (const auto& [id, depth] : depthMap) {
        NodeRef nr;
        nr.id         = id;
        nr.depth      = depth;
        nr.isBoundary = boundaryNodes.count(id) > 0;
        slice.nodes.push_back(nr);
    }

    for (EdgeId eid : includedEdges) {
        slice.edges.push_back({eid});
    }

    return slice;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

GraphSlice callersOf(const Index& index, SymbolId root,
                     int maxDepth,
                     EdgeKindMask edgeFilter,
                     ConfidenceMask confFilter) {
    return bfs(index, root, maxDepth, edgeFilter, confFilter,
               /*forward=*/false, /*includeCallers=*/true, /*includeCallees=*/false);
}

GraphSlice calleesOf(const Index& index, SymbolId root,
                     int maxDepth,
                     EdgeKindMask edgeFilter,
                     ConfidenceMask confFilter) {
    return bfs(index, root, maxDepth, edgeFilter, confFilter,
               /*forward=*/true, /*includeCallers=*/false, /*includeCallees=*/true);
}

GraphSlice neighboursOf(const Index& index, SymbolId root,
                        int maxDepth,
                        EdgeKindMask edgeFilter,
                        ConfidenceMask confFilter) {
    // Run both directions and merge
    GraphSlice callers = callersOf(index, root, maxDepth, edgeFilter, confFilter);
    GraphSlice callees = calleesOf(index, root, maxDepth, edgeFilter, confFilter);

    // Merge into a unified slice
    GraphSlice merged;
    merged.root = root;
    merged.params.root           = root;
    merged.params.maxDepth       = maxDepth;
    merged.params.edgeFilter     = edgeFilter;
    merged.params.confFilter     = confFilter;
    merged.params.includeCallers = true;
    merged.params.includeCallees = true;

    std::unordered_set<SymbolId> seenNodes;
    std::unordered_set<EdgeId>   seenEdges;

    auto mergeSlice = [&](const GraphSlice& s) {
        for (const auto& n : s.nodes) {
            if (seenNodes.insert(n.id).second)
                merged.nodes.push_back(n);
        }
        for (const auto& e : s.edges) {
            if (seenEdges.insert(e.id).second)
                merged.edges.push_back(e);
        }
    };

    mergeSlice(callers);
    mergeSlice(callees);
    return merged;
}

std::vector<SymbolId> findByName(const Index& index,
                                 std::string_view query,
                                 SearchMode mode) {
    switch (mode) {
    case SearchMode::Substring:
        return index.findBySubstring(query);
    case SearchMode::Regex:
        return index.findByRegex(query);
    case SearchMode::ExactQualified:
        return index.findByExact(query);
    }
    return {};
}

} // namespace cg
