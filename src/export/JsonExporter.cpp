#include "export/JsonExporter.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace cg {

static std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 4);
    for (unsigned char c : s) {
        if (c == '"')       out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else if (c < 0x20)  { out += "\\u00"; out += "0123456789abcdef"[c >> 4];
                              out += "0123456789abcdef"[c & 0xF]; }
        else                out += static_cast<char>(c);
    }
    return out;
}

void JsonExporter::exportSlice(const GraphSlice& slice,
                                const Index& index,
                                const ExportHeader& header,
                                std::ostream& out) {
    out << "{\n";

    // Header
    out << "  \"meta\": {\n";
    out << "    \"tool_version\": \"" << jsonEscape(header.toolVersion) << "\",\n";
    out << "    \"source_root\": \"" << jsonEscape(header.sourceRoot) << "\",\n";
    out << "    \"git_commit\": \"" << jsonEscape(header.gitCommit) << "\"";
    if (header.includeTimestamp) {
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::ostringstream ts;
        ts << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%SZ");
        out << ",\n    \"generated\": \"" << ts.str() << "\"";
    }
    out << "\n  },\n";

    // Query params
    {
        const FunctionInfo* rootSym = index.symbol(slice.root);
        out << "  \"query\": {\n";
        out << "    \"root\": \"" << (rootSym ? jsonEscape(rootSym->qualified_name) : "") << "\",\n";
        out << "    \"root_id\": " << slice.root << ",\n";
        out << "    \"max_depth\": " << slice.params.maxDepth << ",\n";
        out << "    \"edge_filter\": " << static_cast<int>(slice.params.edgeFilter) << ",\n";
        out << "    \"confidence_filter\": " << static_cast<int>(slice.params.confFilter) << "\n";
        out << "  },\n";
    }

    // Nodes
    out << "  \"nodes\": [\n";
    bool firstNode = true;
    for (const auto& nr : slice.nodes) {
        if (!firstNode) out << ",\n";
        firstNode = false;

        const FunctionInfo* sym = index.symbol(nr.id);
        out << "    {\n";
        out << "      \"id\": " << nr.id << ",\n";
        out << "      \"qualified_name\": \"" << (sym ? jsonEscape(sym->qualified_name) : "") << "\",\n";
        out << "      \"arity\": " << (sym ? sym->arity : 0) << ",\n";
        out << "      \"class\": \"" << (sym ? jsonEscape(sym->class_qname) : "") << "\",\n";
        out << "      \"is_virtual\": " << (sym && sym->is_virtual ? "true" : "false") << ",\n";
        out << "      \"is_synthetic\": " << (index.isSynthetic(nr.id) ? "true" : "false") << ",\n";
        out << "      \"is_root\": " << (nr.id == slice.root ? "true" : "false") << ",\n";
        out << "      \"is_boundary\": " << (nr.isBoundary ? "true" : "false") << ",\n";
        out << "      \"depth\": " << nr.depth << ",\n";
        if (sym && sym->loc.valid()) {
            out << "      \"loc\": { \"file\": \"" << jsonEscape(sym->loc.file)
                << "\", \"line\": " << sym->loc.line << " }\n";
        } else {
            out << "      \"loc\": null\n";
        }
        out << "    }";
    }
    out << "\n  ],\n";

    // Edges
    out << "  \"edges\": [\n";
    bool firstEdge = true;
    for (const auto& er : slice.edges) {
        const Edge* e = index.edge(er.id);
        if (!e) continue;
        if (!firstEdge) out << ",\n";
        firstEdge = false;

        out << "    {\n";
        out << "      \"id\": " << e->id << ",\n";
        out << "      \"from\": " << e->from << ",\n";
        out << "      \"to\": " << e->to << ",\n";
        out << "      \"kind\": \"" << edgeKindLabel(e->kind) << "\",\n";
        out << "      \"confidence\": \"" << confidenceLabel(e->confidence) << "\",\n";
        if (e->via_macro)
            out << "      \"via_macro\": \"" << jsonEscape(*e->via_macro) << "\",\n";
        if (e->loc.valid()) {
            out << "      \"loc\": { \"file\": \"" << jsonEscape(e->loc.file)
                << "\", \"line\": " << e->loc.line << " }\n";
        } else {
            out << "      \"loc\": null\n";
        }
        out << "    }";
    }
    out << "\n  ]\n";

    out << "}\n";
}

} // namespace cg
