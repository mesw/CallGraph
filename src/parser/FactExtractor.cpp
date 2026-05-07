#include "parser/FactExtractor.h"

#include <tree_sitter/api.h>
#include <algorithm>
#include <cctype>
#include <cstring>

extern "C" const TSLanguage* tree_sitter_cpp();

namespace {
const std::vector<std::string> THREAD_SPAWN_APIS = {
    "CreateThread", "_beginthread", "_beginthreadex",
    "pthread_create", "QThread::create", "std::thread"
};
} // anonymous namespace

namespace cg {

// ---------------------------------------------------------------------------
// Helper: safe node text
// ---------------------------------------------------------------------------

std::string FactExtractor::nodeText(TSNode node) const {
    if (ts_node_is_null(node)) return {};
    uint32_t start = ts_node_start_byte(node);
    uint32_t end_b = ts_node_end_byte(node);
    if (end_b <= start || end_b > static_cast<uint32_t>(SIZE_MAX)) return {};
    return std::string(m_source + start, m_source + end_b);
}

std::string FactExtractor::nodeFieldText(TSNode parent, const char* field) const {
    TSNode child = ts_node_child_by_field_name(parent, field,
                        static_cast<uint32_t>(strlen(field)));
    return nodeText(child);
}

SymbolId FactExtractor::newId() {
    return ++m_nextSymbolId;
}

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------

FactExtractor::FactExtractor(FactBuffer& buffer,
                             std::atomic<uint64_t>& nextSymbolId)
    : m_buffer(buffer)
    , m_nextSymbolId(nextSymbolId)
{}

void FactExtractor::extractFromFile(const std::filesystem::path& path,
                                    const char* source,
                                    std::size_t sourceLen,
                                    TSTree* tree) {
    m_currentFile = path.string();
    m_source      = source;
    (void)sourceLen;

    m_currentNamespace.clear();
    m_currentClass.clear();
    m_currentFunctionId = INVALID_SYMBOL_ID;
    m_insideMacroExpansion = false;

    TSNode root = ts_tree_root_node(tree);
    visitNode(root, 0);
}

// ---------------------------------------------------------------------------
// Recursive visitor
// ---------------------------------------------------------------------------

void FactExtractor::visitNode(TSNode node, int depth) {
    if (ts_node_is_null(node)) return;

    const char* type = ts_node_type(node);

    if (strcmp(type, "function_definition") == 0) {
        extractFunctionDefinition(node);
        return; // The function extractor visits children itself
    }
    if (strcmp(type, "call_expression") == 0) {
        extractCallExpression(node);
        // Still visit children for nested calls
    }
    if (strcmp(type, "assignment_expression") == 0) {
        extractAssignmentExpression(node);
    }
    if (strcmp(type, "preproc_def") == 0) {
        extractPreprocDef(node);
        return;
    }
    if (strcmp(type, "preproc_function_def") == 0) {
        extractPreprocFunctionDef(node);
        return;
    }
    if (strcmp(type, "class_specifier") == 0 ||
        strcmp(type, "struct_specifier") == 0) {
        extractClassSpecifier(node);
        return;
    }
    if (strcmp(type, "namespace_definition") == 0) {
        // Track namespace context
        TSNode nameNode = ts_node_child_by_field_name(node, "name", 4);
        std::string nsName = nodeText(nameNode);
        std::string prevNs = m_currentNamespace;
        if (!nsName.empty()) {
            m_currentNamespace = m_currentNamespace.empty()
                ? nsName : m_currentNamespace + "::" + nsName;
        }
        TSNode body = ts_node_child_by_field_name(node, "body", 4);
        if (!ts_node_is_null(body)) {
            uint32_t childCount = ts_node_child_count(body);
            for (uint32_t i = 0; i < childCount; ++i)
                visitNode(ts_node_child(body, i), depth + 1);
        }
        m_currentNamespace = prevNs;
        return;
    }

    uint32_t childCount = ts_node_child_count(node);
    for (uint32_t i = 0; i < childCount; ++i)
        visitNode(ts_node_child(node, i), depth + 1);
}

// ---------------------------------------------------------------------------
// Build a qualified name from an identifier and the current scope context
// ---------------------------------------------------------------------------

std::string FactExtractor::buildQualifiedName(const std::string& name) const {
    if (!m_currentClass.empty()) {
        if (!m_currentNamespace.empty())
            return m_currentNamespace + "::" + m_currentClass + "::" + name;
        return m_currentClass + "::" + name;
    }
    if (!m_currentNamespace.empty())
        return m_currentNamespace + "::" + name;
    return name;
}

// ---------------------------------------------------------------------------
// Extract function declarations / definitions
// ---------------------------------------------------------------------------

static std::string extractUnqualifiedName(const std::string& qname) {
    auto pos = qname.rfind("::");
    return pos == std::string::npos ? qname : qname.substr(pos + 2);
}

void FactExtractor::extractFunctionDefinition(TSNode node) {
    // declarator field contains the function_declarator
    TSNode declarator = ts_node_child_by_field_name(node, "declarator", 10);
    if (ts_node_is_null(declarator)) {
        // Visit children and return
        uint32_t cc = ts_node_child_count(node);
        for (uint32_t i = 0; i < cc; ++i) visitNode(ts_node_child(node, i), 0);
        return;
    }

    // Navigate to the innermost declarator to get the name
    // function_declarator -> declarator = (qualified_identifier | identifier | ...)
    TSNode innerDecl = declarator;
    while (true) {
        const char* t = ts_node_type(innerDecl);
        if (strcmp(t, "function_declarator") == 0) {
            TSNode inner = ts_node_child_by_field_name(innerDecl, "declarator", 10);
            if (!ts_node_is_null(inner)) { innerDecl = inner; continue; }
        }
        if (strcmp(t, "pointer_declarator") == 0 ||
            strcmp(t, "reference_declarator") == 0) {
            TSNode inner = ts_node_child_by_field_name(innerDecl, "declarator", 10);
            if (!ts_node_is_null(inner)) { innerDecl = inner; continue; }
        }
        break;
    }

    std::string rawName = nodeText(innerDecl);
    if (rawName.empty()) {
        uint32_t cc = ts_node_child_count(node);
        for (uint32_t i = 0; i < cc; ++i) visitNode(ts_node_child(node, i), 0);
        return;
    }

    // Qualified name: if rawName already contains "::", use it as-is
    // otherwise qualify with current scope
    std::string qname;
    if (rawName.find("::") != std::string::npos) {
        if (!m_currentNamespace.empty() &&
            rawName.find(m_currentNamespace) == std::string::npos) {
            qname = m_currentNamespace + "::" + rawName;
        } else {
            qname = rawName;
        }
    } else {
        qname = buildQualifiedName(rawName);
    }

    // Count parameters
    TSNode funcDecl = declarator;
    while (strcmp(ts_node_type(funcDecl), "function_declarator") != 0) {
        TSNode c = ts_node_child_by_field_name(funcDecl, "declarator", 10);
        if (ts_node_is_null(c)) break;
        funcDecl = c;
    }
    int arity = 0;
    if (strcmp(ts_node_type(funcDecl), "function_declarator") == 0) {
        TSNode params = ts_node_child_by_field_name(funcDecl, "parameters", 10);
        if (!ts_node_is_null(params)) {
            uint32_t pc = ts_node_child_count(params);
            for (uint32_t i = 0; i < pc; ++i) {
                const char* pt = ts_node_type(ts_node_child(params, i));
                if (strcmp(pt, "parameter_declaration") == 0 ||
                    strcmp(pt, "variadic_parameter") == 0)
                    ++arity;
            }
        }
    }

    // Check for virtual / static in type specifiers
    bool isVirtual = false, isStatic = false;
    uint32_t cc = ts_node_child_count(node);
    for (uint32_t i = 0; i < cc; ++i) {
        TSNode ch = ts_node_child(node, i);
        const char* ct = ts_node_type(ch);
        if (strcmp(ct, "virtual") == 0) isVirtual = true;
        if (strcmp(ct, "storage_class_specifier") == 0) {
            std::string s = nodeText(ch);
            if (s == "static") isStatic = true;
        }
    }

    TSPoint pt = ts_node_start_point(node);
    SourceLocation loc;
    loc.file   = m_currentFile;
    loc.line   = static_cast<int>(pt.row) + 1;
    loc.column = static_cast<int>(pt.column) + 1;

    FunctionDef def;
    def.id               = newId();
    def.qualified_name   = qname;
    def.unqualified_name = extractUnqualifiedName(qname);
    def.arity            = arity;
    def.class_qname      = m_currentClass;
    def.is_virtual       = isVirtual;
    def.is_static        = isStatic;
    def.is_definition    = true;
    def.loc              = loc;
    SymbolId thisId      = def.id; // capture before move
    m_buffer.appendFunctionDef(std::move(def));

    SymbolId prevFn = m_currentFunctionId;
    m_currentFunctionId = thisId;

    // Visit function body
    TSNode body = ts_node_child_by_field_name(node, "body", 4);
    if (!ts_node_is_null(body)) {
        uint32_t bc = ts_node_child_count(body);
        for (uint32_t i = 0; i < bc; ++i)
            visitNode(ts_node_child(body, i), 0);
    }

    m_currentFunctionId = prevFn;
}

// ---------------------------------------------------------------------------
// Extract call expressions
// ---------------------------------------------------------------------------

std::string FactExtractor::extractCalleeText(TSNode calleeNode) const {
    const char* t = ts_node_type(calleeNode);
    // field_expression: obj.method or ptr->method — grab field
    if (strcmp(t, "field_expression") == 0) {
        TSNode field = ts_node_child_by_field_name(calleeNode, "field", 5);
        return nodeText(field);
    }
    return nodeText(calleeNode);
}

int FactExtractor::countArguments(TSNode argumentList) const {
    if (ts_node_is_null(argumentList)) return 0;
    int count = 0;
    uint32_t cc = ts_node_child_count(argumentList);
    for (uint32_t i = 0; i < cc; ++i) {
        TSNode ch = ts_node_child(argumentList, i);
        const char* ct = ts_node_type(ch);
        // Exclude punctuation: '(', ')', ','
        if (strcmp(ct, ",") != 0 &&
            strcmp(ct, "(") != 0 &&
            strcmp(ct, ")") != 0 &&
            strcmp(ct, "comment") != 0) {
            ++count;
        }
    }
    return count;
}

bool FactExtractor::isThreadSpawnArg(TSNode callNode, TSNode argNode) const {
    // Get the callee of the outer call
    TSNode callee = ts_node_child_by_field_name(callNode, "function", 8);
    if (ts_node_is_null(callee)) return false;
    std::string calleeName = nodeText(callee);
    for (const auto& api : THREAD_SPAWN_APIS) {
        if (calleeName.find(api) != std::string::npos) return true;
    }
    return false;
}

void FactExtractor::extractCallExpression(TSNode node) {
    if (m_currentFunctionId == INVALID_SYMBOL_ID) return;

    TSNode calleeNode = ts_node_child_by_field_name(node, "function", 8);
    TSNode argListNode = ts_node_child_by_field_name(node, "arguments", 9);

    if (ts_node_is_null(calleeNode)) return;

    std::string calleeName = extractCalleeText(calleeNode);
    if (calleeName.empty()) return;

    // Filter out operators and pure numeric literals
    if (calleeName[0] == '"' || (std::isdigit(static_cast<unsigned char>(calleeName[0])))) return;

    int arity = countArguments(argListNode);

    EdgeKind kind = EdgeKind::DirectCall;
    const char* calleeType = ts_node_type(calleeNode);
    if (strcmp(calleeType, "field_expression") == 0) {
        kind = EdgeKind::MethodCall;
    }

    TSPoint pt = ts_node_start_point(node);
    SourceLocation loc;
    loc.file   = m_currentFile;
    loc.line   = static_cast<int>(pt.row) + 1;
    loc.column = static_cast<int>(pt.column) + 1;

    // Check for function pointer arguments that are thread-spawn APIs
    if (!ts_node_is_null(argListNode)) {
        uint32_t ac = ts_node_child_count(argListNode);
        for (uint32_t i = 0; i < ac; ++i) {
            TSNode arg = ts_node_child(argListNode, i);
            const char* at = ts_node_type(arg);
            // If argument is &identifier or identifier that is a function
            if (strcmp(at, "unary_expression") == 0 ||
                strcmp(at, "identifier") == 0) {
                // Check if the outer call is a thread API
                bool threadSpawn = false;
                for (const auto& api : THREAD_SPAWN_APIS) {
                    if (calleeName.find(api) != std::string::npos) {
                        threadSpawn = true;
                        break;
                    }
                }
                if (threadSpawn) {
                    std::string fnName = nodeText(arg);
                    if (!fnName.empty() && fnName[0] == '&') fnName = fnName.substr(1);
                    if (!fnName.empty()) {
                        FuncPtrAssign assign;
                        assign.enclosing_function = m_currentFunctionId;
                        assign.source_function    = fnName;
                        assign.target_expression  = calleeName;
                        assign.loc                = loc;
                        assign.is_thread_spawn    = true;
                        m_buffer.appendFuncPtrAssign(std::move(assign));
                    }
                }
            }
        }
    }

    CallSite cs;
    cs.enclosing_function      = m_currentFunctionId;
    cs.callee_name             = calleeName;
    cs.callee_arity            = arity;
    cs.kind                    = kind;
    cs.loc                     = loc;
    cs.inside_macro_expansion  = m_insideMacroExpansion;
    m_buffer.appendCallSite(std::move(cs));
}

// ---------------------------------------------------------------------------
// Extract function pointer assignments
// ---------------------------------------------------------------------------

void FactExtractor::extractAssignmentExpression(TSNode node) {
    if (m_currentFunctionId == INVALID_SYMBOL_ID) return;

    TSNode lhs = ts_node_child_by_field_name(node, "left", 4);
    TSNode rhs = ts_node_child_by_field_name(node, "right", 5);
    if (ts_node_is_null(rhs)) return;

    std::string rhsText = nodeText(rhs);

    // RHS is &identifier
    bool isFuncPtr = false;
    std::string funcName;
    const char* rt = ts_node_type(rhs);
    if (strcmp(rt, "unary_expression") == 0) {
        // Check for & operator
        TSNode op = ts_node_child(rhs, 0);
        if (!ts_node_is_null(op) && nodeText(op) == "&") {
            TSNode operand = ts_node_child_by_field_name(rhs, "argument", 8);
            funcName = nodeText(operand);
            isFuncPtr = !funcName.empty();
        }
    }

    if (!isFuncPtr) return;

    std::string lhsText = nodeText(lhs);

    TSPoint pt = ts_node_start_point(node);
    SourceLocation loc;
    loc.file   = m_currentFile;
    loc.line   = static_cast<int>(pt.row) + 1;
    loc.column = static_cast<int>(pt.column) + 1;

    FuncPtrAssign assign;
    assign.enclosing_function = m_currentFunctionId;
    assign.target_expression  = lhsText;
    assign.source_function    = funcName;
    assign.loc                = loc;
    assign.is_thread_spawn    = false;
    m_buffer.appendFuncPtrAssign(std::move(assign));
}

// ---------------------------------------------------------------------------
// Extract macro definitions
// ---------------------------------------------------------------------------

void FactExtractor::extractBodyCalls(const std::string& body,
                                     std::vector<std::string>& calls) const {
    // Manual scan for identifier( patterns in the macro body text.
    // We avoid std::regex per the project conventions (no regex in parser layer).
    const char* p   = body.c_str();
    const char* end = p + body.size();
    while (p < end) {
        // Skip until we find the start of an identifier
        if (!std::isalpha(static_cast<unsigned char>(*p)) && *p != '_') { ++p; continue; }
        const char* start = p;
        while (p < end && (std::isalnum(static_cast<unsigned char>(*p)) || *p == '_')) ++p;
        std::string ident(start, p);
        // Skip whitespace
        const char* q = p;
        while (q < end && (*q == ' ' || *q == '\t')) ++q;
        if (q < end && *q == '(') {
            // Looks like a function call token inside the macro body
            if (!ident.empty())
                calls.push_back(ident);
        }
    }
}

void FactExtractor::extractPreprocDef(TSNode node) {
    TSNode nameNode = ts_node_child_by_field_name(node, "name", 4);
    TSNode valueNode = ts_node_child_by_field_name(node, "value", 5);

    std::string name = nodeText(nameNode);
    if (name.empty()) return;

    std::string body = nodeText(valueNode);

    TSPoint pt = ts_node_start_point(node);
    SourceLocation loc;
    loc.file   = m_currentFile;
    loc.line   = static_cast<int>(pt.row) + 1;
    loc.column = static_cast<int>(pt.column) + 1;

    MacroDef md;
    md.name = name;
    md.body = body;
    md.loc  = loc;
    extractBodyCalls(body, md.body_calls);
    m_buffer.appendMacroDef(std::move(md));
}

void FactExtractor::extractPreprocFunctionDef(TSNode node) {
    TSNode nameNode = ts_node_child_by_field_name(node, "name", 4);
    TSNode valueNode = ts_node_child_by_field_name(node, "value", 5);

    std::string name = nodeText(nameNode);
    if (name.empty()) return;

    std::string body = nodeText(valueNode);

    TSPoint pt = ts_node_start_point(node);
    SourceLocation loc;
    loc.file   = m_currentFile;
    loc.line   = static_cast<int>(pt.row) + 1;
    loc.column = static_cast<int>(pt.column) + 1;

    MacroDef md;
    md.name = name;
    md.body = body;
    md.loc  = loc;
    extractBodyCalls(body, md.body_calls);
    m_buffer.appendMacroDef(std::move(md));
}

// ---------------------------------------------------------------------------
// Extract class / struct declarations
// ---------------------------------------------------------------------------

void FactExtractor::extractClassSpecifier(TSNode node) {
    TSNode nameNode = ts_node_child_by_field_name(node, "name", 4);
    std::string className = nodeText(nameNode);
    if (className.empty()) return;

    std::string qname = buildQualifiedName(className);

    // Extract base classes
    std::vector<std::string> bases;
    uint32_t cc = ts_node_child_count(node);
    for (uint32_t i = 0; i < cc; ++i) {
        TSNode ch = ts_node_child(node, i);
        if (strcmp(ts_node_type(ch), "base_class_clause") == 0) {
            uint32_t bc = ts_node_child_count(ch);
            for (uint32_t j = 0; j < bc; ++j) {
                TSNode base = ts_node_child(ch, j);
                const char* bt = ts_node_type(base);
                if (strcmp(bt, "type_identifier") == 0 ||
                    strcmp(bt, "qualified_identifier") == 0 ||
                    strcmp(bt, "template_type") == 0) {
                    bases.push_back(nodeText(base));
                }
            }
        }
    }

    TSPoint pt = ts_node_start_point(node);
    SourceLocation loc;
    loc.file   = m_currentFile;
    loc.line   = static_cast<int>(pt.row) + 1;
    loc.column = static_cast<int>(pt.column) + 1;

    ClassDecl cd;
    cd.qualified_name = qname;
    cd.base_classes   = std::move(bases);
    cd.loc            = loc;
    m_buffer.appendClassDecl(std::move(cd));

    // Visit body with updated class context
    std::string prevClass = m_currentClass;
    m_currentClass = className;

    TSNode body = ts_node_child_by_field_name(node, "body", 4);
    if (!ts_node_is_null(body)) {
        uint32_t bc = ts_node_child_count(body);
        for (uint32_t i = 0; i < bc; ++i)
            visitNode(ts_node_child(body, i), 0);
    }

    m_currentClass = prevClass;
}

} // namespace cg
