#include "export/MermaidExporter.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <unordered_map>

namespace cg {

static std::string sanitiseMermaidId(const std::string& name) {
    // Mermaid node IDs must be alphanumeric + underscore
    std::string id = name;
    for (char& c : id) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_')
            c = '_';
    }
    return id;
}

static std::string sanitiseMermaidLabel(const std::string& name) {
    // Escape quotes in labels
    std::string out;
    out.reserve(name.size());
    for (char c : name) {
        if (c == '"') out += "\\\"";
        else out += c;
    }
    return out;
}

static const char* mermaidEdgeStyle(EdgeKind kind) {
    switch (kind) {
    case EdgeKind::DirectCall:       return " --> ";
    case EdgeKind::MethodCall:       return " --> ";
    case EdgeKind::VirtualCandidate: return " -. VirtualCandidate .-> ";
    case EdgeKind::StoredAsPointer:  return " -. StoredAsPointer .-> ";
    case EdgeKind::PassedToThread:   return " == PassedToThread ==> ";
    case EdgeKind::ViaMacro:         return " -- ViaMacro --> ";
    case EdgeKind::Unresolved:       return " -.-> ";
    }
    return " --> ";
}

void MermaidExporter::exportSlice(const GraphSlice& slice,
                                  const Index& index,
                                  const ExportHeader& header,
                                  std::ostream& out) {
    // Header comment
    out << "%% CallGraph v" << header.toolVersion << "\n";
    out << "%% Source root: " << header.sourceRoot << "\n";
    out << "%% Git commit: " << header.gitCommit << "\n";
    out << "%% Root: ";
    if (auto* sym = index.symbol(slice.root))
        out << sym->qualified_name;
    out << "  depth: " << slice.params.maxDepth << "\n";
    if (header.includeTimestamp) {
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        out << "%% Generated: " << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%SZ") << "\n";
    }
    out << "\n";

    out << "flowchart TD\n";

    // Node declarations
    std::unordered_map<SymbolId, std::string> nodeIds;
    int counter = 0;
    for (const auto& nr : slice.nodes) {
        const FunctionInfo* sym = index.symbol(nr.id);
        std::string label = sym ? sanitiseMermaidLabel(sym->qualified_name) : "?";
        std::string nodeId = "N" + std::to_string(counter++);
        nodeIds[nr.id] = nodeId;

        if (nr.id == slice.root) {
            out << "    " << nodeId << "[\"" << label << "\"]:::root\n";
        } else if (index.isSynthetic(nr.id)) {
            out << "    " << nodeId << "([\"" << label << "\"]):::unresolved\n";
        } else if (nr.isBoundary) {
            out << "    " << nodeId << "[[\"" << label << "\"]]:::boundary\n";
        } else {
            out << "    " << nodeId << "[\"" << label << "\"]\n";
        }
    }

    // Edges
    for (const auto& er : slice.edges) {
        const Edge* e = index.edge(er.id);
        if (!e) continue;
        auto fromIt = nodeIds.find(e->from);
        auto toIt   = nodeIds.find(e->to);
        if (fromIt == nodeIds.end() || toIt == nodeIds.end()) continue;

        out << "    " << fromIt->second
            << mermaidEdgeStyle(e->kind)
            << toIt->second << "\n";
    }

    // Style classes
    out << "\n";
    out << "    classDef root fill:#f90,stroke:#333,stroke-width:2px\n";
    out << "    classDef unresolved fill:#eee,stroke:#999,stroke-dasharray:5 5\n";
    out << "    classDef boundary fill:#cdf,stroke:#69c\n";
}

} // namespace cg
