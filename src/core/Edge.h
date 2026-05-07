#pragma once

#include "core/Types.h"
#include <optional>
#include <string>

namespace cg {

struct Edge {
    EdgeId     id         = INVALID_EDGE_ID;
    SymbolId   from       = INVALID_SYMBOL_ID;
    SymbolId   to         = INVALID_SYMBOL_ID;
    EdgeKind   kind       = EdgeKind::DirectCall;
    Confidence confidence = Confidence::Exact;
    SourceLocation loc;
    std::optional<std::string> via_macro;
};

} // namespace cg
