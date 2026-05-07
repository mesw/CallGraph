#include "export/DotExporter.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <unordered_map>

namespace cg {

static std::string dotEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else out += c;
    }
    return out;
}

struct EdgeStyle {
    const char* style;
    const char* color;
    const char* label;
    const char* penwidth;
};

static EdgeStyle dotEdgeStyle(EdgeKind kind) {
    switch (kind) {
    case EdgeKind::DirectCall:       return {"solid",  "black",     "",             "1"};
    case EdgeKind::MethodCall:       return {"solid",  "black",     "",             "1"};
    case EdgeKind::VirtualCandidate: return {"dashed", "blue",      "virtual",      "1"};
    case EdgeKind::StoredAsPointer:  return {"dotted", "darkgreen", "ptr",          "1"};
    case EdgeKind::PassedToThread:   return {"dotted", "red",       "thread",       "2"};
    case EdgeKind::ViaMacro:         return {"solid",  "grey",      "↻ macro", "1"};
    case EdgeKind::Unresolved:       return {"solid",  "lightgrey", "unresolved",   "1"};
    }
    return {"solid", "black", "", "1"};
}

void DotExporter::exportSlice(const GraphSlice& slice,
                               const Index& index,
                               const ExportHeader& header,
                               std::ostream& out) {
    out << "// CallGraph v" << header.toolVersion << "\n";
    out << "// Source root: " << header.sourceRoot << "\n";
    out << "// Git commit: " << header.gitCommit << "\n";
    if (auto* sym = index.symbol(slice.root))
        out << "// Root: " << sym->qualified_name << "  depth: " << slice.params.maxDepth << "\n";
    if (header.includeTimestamp) {
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        out << "// Generated: " << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%SZ") << "\n";
    }
    out << "\n";

    out << "digraph CallGraph {\n";
    out << "    rankdir=TB;\n";
    out << "    splines=true;\n";
    out << "    overlap=false;\n";
    out << "    nodesep=0.3;\n";
    out << "    ranksep=0.5;\n\n";

    // Nodes
    std::unordered_map<SymbolId, std::string> nodeIds;
    int counter = 0;
    for (const auto& nr : slice.nodes) {
        const FunctionInfo* sym = index.symbol(nr.id);
        std::string label = sym ? dotEscape(sym->qualified_name) : "?";
        std::string nodeId = "n" + std::to_string(counter++);
        nodeIds[nr.id] = nodeId;

        out << "    " << nodeId << " [";
        out << "label=\"" << label << "\"";

        if (nr.id == slice.root) {
            out << ", style=filled, fillcolor=orange";
        } else if (index.isSynthetic(nr.id)) {
            out << ", style=\"filled,dashed\", fillcolor=lightgrey, color=grey";
        } else if (nr.isBoundary) {
            out << ", style=filled, fillcolor=lightblue";
        }

        // Shape: method → box, free function → ellipse
        if (sym && !sym->class_qname.empty())
            out << ", shape=box";
        else
            out << ", shape=ellipse";

        out << "];\n";
    }

    out << "\n";

    // Edges
    for (const auto& er : slice.edges) {
        const Edge* e = index.edge(er.id);
        if (!e) continue;
        auto fromIt = nodeIds.find(e->from);
        auto toIt   = nodeIds.find(e->to);
        if (fromIt == nodeIds.end() || toIt == nodeIds.end()) continue;

        EdgeStyle es = dotEdgeStyle(e->kind);
        out << "    " << fromIt->second << " -> " << toIt->second << " [";
        out << "style=" << es.style;
        out << ", color=" << es.color;
        out << ", penwidth=" << es.penwidth;
        if (es.label && es.label[0] != '\0')
            out << ", label=\"" << es.label << "\"";
        out << "];\n";
    }

    out << "}\n";
}

} // namespace cg
