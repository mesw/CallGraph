#include "resolver/Resolver.h"

#include <QtTest/QtTest>

using namespace cg;

class TestResolver : public QObject {
    Q_OBJECT

private slots:
    void resolveExactCall();
    void resolveUnknownCall();
    void resolveOverloadedCall();
    void resolveVirtualExpansion();
    void resolveFuncPtrAssign();
    void resolveThreadSpawnAssign();

private:
    static FunctionDef makeDef(SymbolId id, const std::string& qname,
                                int arity = 0, bool isVirtual = false,
                                const std::string& classQName = "") {
        FunctionDef d;
        d.id               = id;
        d.qualified_name   = qname;
        d.unqualified_name = qname.rfind("::") != std::string::npos
                             ? qname.substr(qname.rfind("::") + 2) : qname;
        d.arity            = arity;
        d.is_definition    = true;
        d.is_virtual       = isVirtual;
        d.class_qname      = classQName;
        d.loc.file         = "test.cpp";
        d.loc.line         = 1;
        return d;
    }

    static CallSite makeCall(SymbolId caller, const std::string& callee, int arity = 0) {
        CallSite cs;
        cs.enclosing_function = caller;
        cs.callee_name        = callee;
        cs.callee_arity       = arity;
        cs.kind               = EdgeKind::DirectCall;
        cs.loc.file           = "test.cpp";
        cs.loc.line           = 10;
        return cs;
    }
};

void TestResolver::resolveExactCall() {
    FactBuffer facts;
    facts.functionDefs.push_back(makeDef(1, "foo"));
    facts.functionDefs.push_back(makeDef(2, "bar"));
    facts.callSites.push_back(makeCall(1, "bar"));

    Resolver resolver;
    auto index = resolver.resolve(facts);

    QCOMPARE(index->symbolCount(), 2u);
    QCOMPARE(index->edgeCount(), 1u);

    const auto& edges = index->allEdges();
    QCOMPARE(edges[0].from, 1u);
    QCOMPARE(edges[0].to, 2u);
    QCOMPARE(edges[0].kind, EdgeKind::DirectCall);
    QCOMPARE(edges[0].confidence, Confidence::Exact);
}

void TestResolver::resolveUnknownCall() {
    FactBuffer facts;
    facts.functionDefs.push_back(makeDef(1, "foo"));
    facts.callSites.push_back(makeCall(1, "external_func"));

    Resolver resolver;
    auto index = resolver.resolve(facts);

    QCOMPARE(index->edgeCount(), 1u);
    const auto& edges = index->allEdges();
    QCOMPARE(edges[0].kind, EdgeKind::Unresolved);
    QCOMPARE(edges[0].confidence, Confidence::Unknown);
    QVERIFY(index->isSynthetic(edges[0].to));
}

void TestResolver::resolveOverloadedCall() {
    FactBuffer facts;
    // Two overloads of "foo" with different arities
    facts.functionDefs.push_back(makeDef(1, "caller"));
    FunctionDef f1 = makeDef(2, "foo", 0);
    FunctionDef f2 = makeDef(3, "foo", 1);
    facts.functionDefs.push_back(f1);
    facts.functionDefs.push_back(f2);

    // Call with arity=0 should resolve to exactly f1
    facts.callSites.push_back(makeCall(1, "foo", 0));

    Resolver resolver;
    auto index = resolver.resolve(facts);

    // Should find 1 edge to the arity=0 overload
    bool foundExact = false;
    for (const auto& e : index->allEdges()) {
        if (e.from == 1 && e.to == 2) {
            QCOMPARE(e.confidence, Confidence::Exact);
            foundExact = true;
        }
    }
    QVERIFY(foundExact);
}

void TestResolver::resolveVirtualExpansion() {
    FactBuffer facts;
    // Base class with virtual method
    facts.functionDefs.push_back(makeDef(1, "caller"));
    facts.functionDefs.push_back(makeDef(2, "Base::doWork", 0, /*isVirtual=*/true, "Base"));
    facts.functionDefs.push_back(makeDef(3, "Derived::doWork", 0, false, "Derived"));

    ClassDecl base;
    base.qualified_name = "Base";
    base.loc.file = "test.cpp"; base.loc.line = 1;
    facts.classDecls.push_back(base);

    ClassDecl derived;
    derived.qualified_name = "Derived";
    derived.base_classes   = {"Base"};
    derived.loc.file = "test.cpp"; derived.loc.line = 5;
    facts.classDecls.push_back(derived);

    CallSite cs = makeCall(1, "Base::doWork", 0);
    cs.kind = EdgeKind::MethodCall;
    facts.callSites.push_back(cs);

    ResolverOptions opts;
    opts.enableVirtualExpansion = true;
    Resolver resolver(opts);
    auto index = resolver.resolve(facts);

    bool foundVirtual = false;
    for (const auto& e : index->allEdges()) {
        if (e.kind == EdgeKind::VirtualCandidate && e.to == 3) {
            foundVirtual = true;
            QCOMPARE(e.confidence, Confidence::Heuristic);
        }
    }
    QVERIFY(foundVirtual);
}

void TestResolver::resolveFuncPtrAssign() {
    FactBuffer facts;
    facts.functionDefs.push_back(makeDef(1, "setup"));
    facts.functionDefs.push_back(makeDef(2, "handler"));

    FuncPtrAssign assign;
    assign.enclosing_function = 1;
    assign.source_function    = "handler";
    assign.target_expression  = "g_handler";
    assign.is_thread_spawn    = false;
    assign.loc.file           = "test.cpp";
    assign.loc.line           = 5;
    facts.funcPtrAssigns.push_back(assign);

    Resolver resolver;
    auto index = resolver.resolve(facts);

    bool found = false;
    for (const auto& e : index->allEdges()) {
        if (e.kind == EdgeKind::StoredAsPointer && e.from == 1 && e.to == 2) {
            found = true;
        }
    }
    QVERIFY(found);
}

void TestResolver::resolveThreadSpawnAssign() {
    FactBuffer facts;
    facts.functionDefs.push_back(makeDef(1, "start"));
    facts.functionDefs.push_back(makeDef(2, "workerThread"));

    FuncPtrAssign assign;
    assign.enclosing_function = 1;
    assign.source_function    = "workerThread";
    assign.target_expression  = "CreateThread";
    assign.is_thread_spawn    = true;
    assign.loc.file           = "test.cpp";
    assign.loc.line           = 8;
    facts.funcPtrAssigns.push_back(assign);

    Resolver resolver;
    auto index = resolver.resolve(facts);

    bool found = false;
    for (const auto& e : index->allEdges()) {
        if (e.kind == EdgeKind::PassedToThread && e.from == 1 && e.to == 2) {
            found = true;
        }
    }
    QVERIFY(found);
}

QTEST_MAIN(TestResolver)
#include "test_resolver.moc"
