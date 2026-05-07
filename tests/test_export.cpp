#include "export/DotExporter.h"
#include "export/DrawioExporter.h"
#include "export/JsonExporter.h"
#include "export/MermaidExporter.h"
#include "query/QueryEngine.h"
#include "resolver/Resolver.h"

#include <QtTest/QtTest>
#include <sstream>

using namespace cg;

class TestExport : public QObject {
    Q_OBJECT

private slots:
    void mermaidContainsNodes();
    void mermaidContainsEdge();
    void dotContainsDigraph();
    void drawioContainsXml();
    void jsonContainsNodes();
    void jsonContainsEdges();
    void allFormatsHaveHeader();

private:
    static std::unique_ptr<Index> buildIndex() {
        FactBuffer facts;
        auto def = [](SymbolId id, const char* name) {
            FunctionDef d; d.id = id; d.qualified_name = name;
            d.unqualified_name = name; d.is_definition = true;
            d.loc.file = "test.cpp"; d.loc.line = 1;
            return d;
        };
        facts.functionDefs.push_back(def(1, "A"));
        facts.functionDefs.push_back(def(2, "B"));
        CallSite cs; cs.enclosing_function = 1; cs.callee_name = "B";
        cs.callee_arity = 0; cs.kind = EdgeKind::DirectCall;
        cs.loc.file = "test.cpp"; cs.loc.line = 3;
        facts.callSites.push_back(cs);
        Resolver resolver;
        return resolver.resolve(facts);
    }

    static ExportHeader makeHeader() {
        ExportHeader h;
        h.toolVersion = "1.0.0";
        h.sourceRoot  = "/src";
        h.gitCommit   = "abc123";
        h.includeTimestamp = false;
        return h;
    }

    static GraphSlice makeSlice(const Index& idx) {
        return neighboursOf(idx, 1, 3);
    }
};

void TestExport::mermaidContainsNodes() {
    auto idx = buildIndex();
    auto slice = makeSlice(*idx);
    std::ostringstream oss;
    MermaidExporter exp;
    exp.exportSlice(slice, *idx, makeHeader(), oss);
    auto out = oss.str();
    QVERIFY(out.find("flowchart TD") != std::string::npos);
    QVERIFY(out.find("A") != std::string::npos);
    QVERIFY(out.find("B") != std::string::npos);
}

void TestExport::mermaidContainsEdge() {
    auto idx = buildIndex();
    auto slice = makeSlice(*idx);
    std::ostringstream oss;
    MermaidExporter exp;
    exp.exportSlice(slice, *idx, makeHeader(), oss);
    auto out = oss.str();
    // Should have an arrow between some node IDs
    QVERIFY(out.find("-->") != std::string::npos);
}

void TestExport::dotContainsDigraph() {
    auto idx = buildIndex();
    auto slice = makeSlice(*idx);
    std::ostringstream oss;
    DotExporter exp;
    exp.exportSlice(slice, *idx, makeHeader(), oss);
    auto out = oss.str();
    QVERIFY(out.find("digraph CallGraph") != std::string::npos);
    QVERIFY(out.find("->") != std::string::npos);
}

void TestExport::drawioContainsXml() {
    auto idx = buildIndex();
    auto slice = makeSlice(*idx);
    std::ostringstream oss;
    DrawioExporter exp;
    exp.exportSlice(slice, *idx, makeHeader(), oss);
    auto out = oss.str();
    QVERIFY(out.find("mxGraphModel") != std::string::npos);
    QVERIFY(out.find("mxCell") != std::string::npos);
}

void TestExport::jsonContainsNodes() {
    auto idx = buildIndex();
    auto slice = makeSlice(*idx);
    std::ostringstream oss;
    JsonExporter exp;
    exp.exportSlice(slice, *idx, makeHeader(), oss);
    auto out = oss.str();
    QVERIFY(out.find("\"nodes\"") != std::string::npos);
    QVERIFY(out.find("\"A\"") != std::string::npos);
    QVERIFY(out.find("\"B\"") != std::string::npos);
}

void TestExport::jsonContainsEdges() {
    auto idx = buildIndex();
    auto slice = makeSlice(*idx);
    std::ostringstream oss;
    JsonExporter exp;
    exp.exportSlice(slice, *idx, makeHeader(), oss);
    auto out = oss.str();
    QVERIFY(out.find("\"edges\"") != std::string::npos);
    QVERIFY(out.find("\"DirectCall\"") != std::string::npos);
}

void TestExport::allFormatsHaveHeader() {
    auto idx = buildIndex();
    auto slice = makeSlice(*idx);
    auto h = makeHeader();

    auto check = [&](const std::string& out) {
        QVERIFY(out.find("1.0.0") != std::string::npos);
        QVERIFY(out.find("abc123") != std::string::npos);
    };

    { std::ostringstream oss; MermaidExporter().exportSlice(slice, *idx, h, oss); check(oss.str()); }
    { std::ostringstream oss; DotExporter().exportSlice(slice, *idx, h, oss); check(oss.str()); }
    { std::ostringstream oss; DrawioExporter().exportSlice(slice, *idx, h, oss); check(oss.str()); }
    { std::ostringstream oss; JsonExporter().exportSlice(slice, *idx, h, oss); check(oss.str()); }
}

QTEST_MAIN(TestExport)
#include "test_export.moc"
