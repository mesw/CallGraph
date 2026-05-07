#pragma once

#include "core/Types.h"
#include <atomic>
#include <mutex>
#include <string>
#include <vector>

namespace cg {

// ---------------------------------------------------------------------------
// Raw fact records produced by the FactExtractor.
// Names are unresolved at this stage — the Resolver handles resolution.
// ---------------------------------------------------------------------------

struct FunctionDef {
    SymbolId    id;                 // assigned during extraction (file-local serial)
    std::string qualified_name;
    std::string unqualified_name;
    int         arity         = 0;
    std::string class_qname;
    bool        is_virtual    = false;
    bool        is_static     = false;
    bool        is_definition = false;
    SourceLocation loc;
};

struct CallSite {
    SymbolId    enclosing_function = INVALID_SYMBOL_ID;
    std::string callee_name;
    int         callee_arity      = 0;
    EdgeKind    kind              = EdgeKind::DirectCall;
    SourceLocation loc;
    bool        inside_macro_expansion = false;
    std::string macro_name;
};

struct FuncPtrAssign {
    SymbolId    enclosing_function = INVALID_SYMBOL_ID;
    std::string target_expression;
    std::string source_function;
    SourceLocation loc;
    bool        is_thread_spawn = false;
};

struct MacroDef {
    std::string name;
    std::string body;
    std::vector<std::string> body_calls;
    SourceLocation loc;
};

struct ClassDecl {
    std::string qualified_name;
    std::vector<std::string> base_classes;
    SourceLocation loc;
};

// ---------------------------------------------------------------------------
// Thread-safe fact buffer (MPSC queue backed by a mutex-protected vector).
// The resolver reads it after all parser threads have finished.
// ---------------------------------------------------------------------------

struct FactBuffer {
    std::mutex mtx;

    std::vector<FunctionDef>   functionDefs;
    std::vector<CallSite>      callSites;
    std::vector<FuncPtrAssign> funcPtrAssigns;
    std::vector<MacroDef>      macroDefs;
    std::vector<ClassDecl>     classDecls;
    std::vector<std::string>   parseErrors; // file paths with errors

    void appendFunctionDef(FunctionDef f) {
        std::lock_guard<std::mutex> lk(mtx);
        functionDefs.push_back(std::move(f));
    }
    void appendCallSite(CallSite c) {
        std::lock_guard<std::mutex> lk(mtx);
        callSites.push_back(std::move(c));
    }
    void appendFuncPtrAssign(FuncPtrAssign a) {
        std::lock_guard<std::mutex> lk(mtx);
        funcPtrAssigns.push_back(std::move(a));
    }
    void appendMacroDef(MacroDef m) {
        std::lock_guard<std::mutex> lk(mtx);
        macroDefs.push_back(std::move(m));
    }
    void appendClassDecl(ClassDecl c) {
        std::lock_guard<std::mutex> lk(mtx);
        classDecls.push_back(std::move(c));
    }
    void appendParseError(const std::string& file) {
        std::lock_guard<std::mutex> lk(mtx);
        parseErrors.push_back(file);
    }
};

} // namespace cg
