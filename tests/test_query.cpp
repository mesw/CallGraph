#include "core/Index.h"
#include "query/QueryEngine.h"
#include "resolver/Resolver.h"

#include <QtTest/QtTest>

using namespace cg;

class TestQuery : public QObject {
    Q_OBJECT

private slots:
    void calleesDepth1();
    void callersDepth1();
    void neighboursUnion();
    void edgeKindFilter();
    void nameSearchSubstring();
    void nameSearchRegex();
    void nameSearchExact();
    void boundaryNodes();

private:
    // Build a simple index: A→B→C (call chain)
    static std::unique_ptr<Index> buildChainIndex() {
        FactBuffer facts;
        auto def = [](SymbolId id, const char* name) {
            FunctionDef d;
            d.id = id; d.qualified_name = name; d.unqualified_name = name;
            d.is_definition = true; d.loc.file = "x.cpp"; d.loc.line = 1;
            return d;
        };
        facts.functionDefs.push_back(def(1, "A"));
        facts.functionDefs.push_back(def(2, "B"));
        facts.functionDefs.push_back(def(3, "C"));

        auto call = [](SymbolId from, const char* name) {
            CallSite cs;
            cs.enclosing_function = from;
            cs.callee_name = name; cs.callee_arity = 0;
            cs.kind = EdgeKind::DirectCall;
            cs.loc.file = "x.cpp"; cs.loc.line = 5;
            return cs;
        };
        facts.callSites.push_back(call(1, "B"));
        facts.callSites.push_back(call(2, "C"));

        Resolver resolver;
        return resolver.resolve(facts);
    }
};

void TestQuery::calleesDepth1() {
    auto idx = buildChainIndex();
    auto slice = calleesOf(*idx, 1, 1);

    // Should contain A (root) and B (1 level callee)
    QVERIFY(slice.hasNode(1));
    QVERIFY(slice.hasNode(2));
    QVERIFY(!slice.hasNode(3)); // too deep
    QCOMPARE(static_cast<int>(slice.edges.size()), 1);
}

void TestQuery::callersDepth1() {
    auto idx = buildChainIndex();
    auto slice = callersOf(*idx, 3, 1);

    QVERIFY(slice.hasNode(3));
    QVERIFY(slice.hasNode(2));
    QVERIFY(!slice.hasNode(1));
    QCOMPARE(static_cast<int>(slice.edges.size()), 1);
}

void TestQuery::neighboursUnion() {
    auto idx = buildChainIndex();
    auto slice = neighboursOf(*idx, 2, 1);

    // B's neighbours at depth 1: A (caller) and C (callee)
    QVERIFY(slice.hasNode(1));
    QVERIFY(slice.hasNode(2));
    QVERIFY(slice.hasNode(3));
    QCOMPARE(static_cast<int>(slice.edges.size()), 2);
}

void TestQuery::edgeKindFilter() {
    FactBuffer facts;
    FunctionDef d; d.id = 1; d.qualified_name = "Caller"; d.unqualified_name = "Caller";
    d.is_definition = true; d.loc.file = "x.cpp"; d.loc.line = 1;
    facts.functionDefs.push_back(d);

    FunctionDef d2; d2.id = 2; d2.qualified_name = "Callee"; d2.unqualified_name = "Callee";
    d2.is_definition = true; d2.loc.file = "x.cpp"; d2.loc.line = 2;
    facts.functionDefs.push_back(d2);

    CallSite cs;
    cs.enclosing_function = 1;
    cs.callee_name = "Callee"; cs.callee_arity = 0;
    cs.kind = EdgeKind::MethodCall;
    cs.loc.file = "x.cpp"; cs.loc.line = 5;
    facts.callSites.push_back(cs);

    Resolver resolver;
    auto idx = resolver.resolve(facts);

    // Filter to DirectCall only — should exclude the MethodCall edge
    EdgeKindMask directOnly = edgeBit(EdgeKind::DirectCall);
    auto slice = calleesOf(*idx, 1, 1, directOnly);
    QCOMPARE(static_cast<int>(slice.edges.size()), 0);

    // Filter to MethodCall — should include it
    EdgeKindMask methodOnly = edgeBit(EdgeKind::MethodCall);
    auto slice2 = calleesOf(*idx, 1, 1, methodOnly);
    QCOMPARE(static_cast<int>(slice2.edges.size()), 1);
}

void TestQuery::nameSearchSubstring() {
    auto idx = buildChainIndex();
    auto results = findByName(*idx, "B", SearchMode::Substring);
    QVERIFY(!results.empty());
    bool found = false;
    for (SymbolId sid : results) {
        if (auto* s = idx->symbol(sid)) {
            if (s->qualified_name == "B") { found = true; break; }
        }
    }
    QVERIFY(found);
}

void TestQuery::nameSearchRegex() {
    auto idx = buildChainIndex();
    auto results = findByName(*idx, "^[AB]$", SearchMode::Regex);
    QCOMPARE(static_cast<int>(results.size()), 2);
}

void TestQuery::nameSearchExact() {
    auto idx = buildChainIndex();
    auto results = findByName(*idx, "C", SearchMode::ExactQualified);
    QCOMPARE(static_cast<int>(results.size()), 1);
    QCOMPARE(idx->symbol(results[0])->qualified_name, std::string("C"));
}

void TestQuery::boundaryNodes() {
    auto idx = buildChainIndex();
    // At depth=1 from A, B is at the boundary (it has callee C beyond depth)
    auto slice = calleesOf(*idx, 1, 1);
    bool bIsBoundary = false;
    for (const auto& n : slice.nodes) {
        if (n.id == 2 && n.isBoundary) { bIsBoundary = true; break; }
    }
    QVERIFY(bIsBoundary);
}

QTEST_MAIN(TestQuery)
#include "test_query.moc"
