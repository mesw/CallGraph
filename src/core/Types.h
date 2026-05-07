#pragma once

#include <cstdint>
#include <string>

namespace cg {

using SymbolId = uint64_t;
using EdgeId   = uint64_t;

constexpr SymbolId INVALID_SYMBOL_ID = 0;
constexpr EdgeId   INVALID_EDGE_ID   = 0;

enum class EdgeKind : uint8_t {
    DirectCall,
    MethodCall,
    VirtualCandidate,
    StoredAsPointer,
    PassedToThread,
    ViaMacro,
    Unresolved
};

enum class Confidence : uint8_t {
    Exact,
    Overload,
    Heuristic,
    Unknown
};

enum class SearchMode : uint8_t {
    Substring,
    Regex,
    ExactQualified
};

// Bitmask helpers for filtering
using EdgeKindMask    = uint8_t;
using ConfidenceMask  = uint8_t;

constexpr EdgeKindMask ALL_EDGE_KINDS = 0xFF;
constexpr ConfidenceMask ALL_CONFIDENCE = 0xFF;

inline EdgeKindMask edgeBit(EdgeKind k) {
    return static_cast<EdgeKindMask>(1u << static_cast<uint8_t>(k));
}
inline ConfidenceMask confidenceBit(Confidence c) {
    return static_cast<ConfidenceMask>(1u << static_cast<uint8_t>(c));
}

struct SourceLocation {
    std::string file;
    int line   = 0;
    int column = 0;

    bool valid() const { return !file.empty() && line > 0; }
};

} // namespace cg
