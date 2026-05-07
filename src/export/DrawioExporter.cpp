#include "export/DrawioExporter.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <cmath>

namespace cg {

static std::string xmlEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        switch (c) {
        case '&':  out += "&amp;";  break;
        case '<':  out += "&lt;";   break;
        case '>':  out += "&gt;";   break;
        case '"':  out += "&quot;"; break;
        case '\'': out += "&apos;"; break;
        default:   out += c;        break;
        }
    }
    return out;
}

struct DrawioEdgeStyle {
    std::string strokeColor;
    std::string dashed;    // "1" or "0"
    std::string strokeWidth;
    std::string label;
};

static DrawioEdgeStyle drawioEdgeStyle(EdgeKind kind) {
    switch (kind) {
    case EdgeKind::DirectCall:
        return {"#000000", "0", "1", ""};
    case EdgeKind::MethodCall:
        return {"#000000", "0", "1", ""};
    case EdgeKind::VirtualCandidate:
        return {"#0000FF", "1", "1", "virtual"};
    case EdgeKind::StoredAsPointer:
        return {"#006400", "1", "1", "ptr"};
    case EdgeKind::PassedToThread:
        return {"#FF0000", "1", "2", "thread"};
    case EdgeKind::ViaMacro:
        return {"#888888", "0", "1", "macro"};
    case EdgeKind::Unresolved:
        return {"#AAAAAA", "0", "1", ""};
    }
    return {"#000000", "0", "1", ""};
}

void DrawioExporter::exportSlice(const GraphSlice& slice,
                                  const Index& index,
                                  const ExportHeader& header,
                                  std::ostream& out) {
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    out << "<!-- CallGraph v" << header.toolVersion << " -->\n";
    out << "<!-- Source root: " << header.sourceRoot << " -->\n";
    out << "<!-- Git commit: " << header.gitCommit << " -->\n";
    if (header.includeTimestamp) {
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        out << "<!-- Generated: " << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%SZ") << " -->\n";
    }
    out << "<mxGraphModel><root>\n";
    out << "  <mxCell id=\"0\"/>\n";
    out << "  <mxCell id=\"1\" parent=\"0\"/>\n";

    // Simple grid layout: columns of 6, 200px wide, 80px tall, 40px gap
    const double cellW = 200.0;
    const double cellH = 60.0;
    const double gapX  = 60.0;
    const double gapY  = 40.0;
    const int    cols  = 6;

    std::unordered_map<SymbolId, std::string> nodeIds;
    int counter = 2;
    int idx = 0;

    for (const auto& nr : slice.nodes) {
        const FunctionInfo* sym = index.symbol(nr.id);
        std::string label = sym ? sym->qualified_name : "?";
        std::string cellId = std::to_string(counter++);
        nodeIds[nr.id] = cellId;

        double x = (idx % cols) * (cellW + gapX);
        double y = (idx / cols) * (cellH + gapY);
        ++idx;

        std::string fillColor = "#FFFFFF";
        if (nr.id == slice.root) fillColor = "#FF9900";
        else if (index.isSynthetic(nr.id)) fillColor = "#EEEEEE";
        else if (nr.isBoundary) fillColor = "#CCE5FF";

        out << "  <mxCell id=\"" << cellId << "\" value=\""
            << xmlEscape(label) << "\""
            << " style=\"rounded=1;whiteSpace=wrap;html=1;fillColor=" << fillColor << ";\""
            << " vertex=\"1\" parent=\"1\">\n";
        out << "    <mxGeometry x=\"" << static_cast<int>(x)
            << "\" y=\"" << static_cast<int>(y)
            << "\" width=\"" << static_cast<int>(cellW)
            << "\" height=\"" << static_cast<int>(cellH)
            << "\" as=\"geometry\"/>\n";
        out << "  </mxCell>\n";
    }

    // Edges
    for (const auto& er : slice.edges) {
        const Edge* e = index.edge(er.id);
        if (!e) continue;
        auto fromIt = nodeIds.find(e->from);
        auto toIt   = nodeIds.find(e->to);
        if (fromIt == nodeIds.end() || toIt == nodeIds.end()) continue;

        DrawioEdgeStyle es = drawioEdgeStyle(e->kind);
        std::string cellId = std::to_string(counter++);

        out << "  <mxCell id=\"" << cellId << "\""
            << " value=\"" << xmlEscape(es.label) << "\""
            << " style=\"edgeStyle=orthogonalEdgeStyle;"
            << "strokeColor=" << es.strokeColor << ";"
            << "dashed=" << es.dashed << ";"
            << "strokeWidth=" << es.strokeWidth << ";\""
            << " edge=\"1\" source=\"" << fromIt->second
            << "\" target=\"" << toIt->second
            << "\" parent=\"1\">\n";
        out << "    <mxGeometry relative=\"1\" as=\"geometry\"/>\n";
        out << "  </mxCell>\n";
    }

    out << "</root></mxGraphModel>\n";
}

} // namespace cg
