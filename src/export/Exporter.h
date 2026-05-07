#pragma once

#include "core/GraphSlice.h"
#include "core/Index.h"
#include <ostream>
#include <string>

namespace cg {

struct ExportHeader {
    std::string toolVersion;
    std::string sourceRoot;
    std::string gitCommit;
    bool includeTimestamp = true;
};

// Returns a human-readable label for an EdgeKind
inline const char* edgeKindLabel(EdgeKind k) {
    switch (k) {
    case EdgeKind::DirectCall:       return "DirectCall";
    case EdgeKind::MethodCall:       return "MethodCall";
    case EdgeKind::VirtualCandidate: return "VirtualCandidate";
    case EdgeKind::StoredAsPointer:  return "StoredAsPointer";
    case EdgeKind::PassedToThread:   return "PassedToThread";
    case EdgeKind::ViaMacro:         return "ViaMacro";
    case EdgeKind::Unresolved:       return "Unresolved";
    }
    return "Unknown";
}

inline const char* confidenceLabel(Confidence c) {
    switch (c) {
    case Confidence::Exact:     return "Exact";
    case Confidence::Overload:  return "Overload";
    case Confidence::Heuristic: return "Heuristic";
    case Confidence::Unknown:   return "Unknown";
    }
    return "Unknown";
}

} // namespace cg
