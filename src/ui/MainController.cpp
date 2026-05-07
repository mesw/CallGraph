#include "ui/MainController.h"
#include "ui/IndexWorker.h"

#include "export/DotExporter.h"
#include "export/DrawioExporter.h"
#include "export/JsonExporter.h"
#include "export/MermaidExporter.h"
#include "query/QueryEngine.h"

#include <QDesktopServices>
#include <QLoggingCategory>
#include <QProcess>
#include <QThread>
#include <QUrl>
#include <fstream>

Q_LOGGING_CATEGORY(lcController, "cg.controller")

namespace cg {

static constexpr int MAX_HISTORY = 50;

MainController::MainController(QObject* parent)
    : QObject(parent)
{
    m_searchModel.setParent(this);
    m_callersModel.setParent(this);
    m_calleesModel.setParent(this);
}

// ---------------------------------------------------------------------------
// Index build
// ---------------------------------------------------------------------------

void MainController::buildIndex() {
    if (m_indexBuilding) return;

    QString root = m_settings.sourceRoot();
    if (root.isEmpty()) {
        m_indexStatus = "No source root configured.";
        emit indexStatusChanged();
        return;
    }

    m_indexReady    = false;
    m_indexBuilding = true;
    m_indexStatus   = "Discovering files…";
    m_parseErrors.clear();
    emit indexReadyChanged();
    emit indexBuildingChanged();
    emit indexStatusChanged();
    emit parseErrorsChanged();

    auto* worker = new IndexWorker(root, m_settings.virtualExpansion());
    auto* thread = new QThread(this);
    worker->moveToThread(thread);

    connect(thread, &QThread::started, worker, &IndexWorker::run);
    connect(worker, &IndexWorker::discoveryProgress, this, [this](int n) {
        m_indexStatus = QString("Discovering… %1 files found").arg(n);
        emit indexStatusChanged();
    }, Qt::QueuedConnection);
    connect(worker, &IndexWorker::parseProgress, this,
            [this](int parsed, int total, int /*errors*/) {
        m_indexStatus = QString("Parsing… %1 / %2").arg(parsed).arg(total);
        emit indexStatusChanged();
    }, Qt::QueuedConnection);
    connect(worker, &IndexWorker::indexReady, this,
            [this](std::shared_ptr<const Index> idx, const QStringList& errors) {
        onIndexReady(std::move(idx), errors);
    }, Qt::QueuedConnection);
    connect(worker, &IndexWorker::finished, this,
            [this](bool ok, const QString& err) {
        onIndexFinished(ok, err);
    }, Qt::QueuedConnection);
    connect(worker, &IndexWorker::finished, thread, &QThread::quit);
    connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);

    thread->start();
}

void MainController::onIndexReady(std::shared_ptr<const Index> index,
                                   const QStringList& errors) {
    m_index = std::move(index);
    m_parseErrors = errors;
    m_searchModel.setIndex(m_index);
    m_callersModel.setIndex(m_index);
    m_calleesModel.setIndex(m_index);
    m_focusId = INVALID_SYMBOL_ID;
    emit focusIdChanged();
    emit parseErrorsChanged();
    emit symbolCountChanged();
    emit edgeCountChanged();
}

void MainController::onIndexFinished(bool success, const QString& errorMessage) {
    m_indexBuilding = false;
    m_indexReady    = success;
    if (success) {
        m_indexStatus = QString("Index ready: %1 symbols, %2 edges")
            .arg(symbolCount()).arg(edgeCount());
    } else {
        m_indexStatus = "Index failed: " + errorMessage;
    }
    emit indexBuildingChanged();
    emit indexReadyChanged();
    emit indexStatusChanged();
}

// ---------------------------------------------------------------------------
// Search
// ---------------------------------------------------------------------------

void MainController::setSearchQuery(const QString& q) {
    if (m_searchQuery == q) return;
    m_searchQuery = q;
    emit searchQueryChanged();
    runSearch();
}

void MainController::setSearchMode(int mode) {
    if (m_searchMode == mode) return;
    m_searchMode = mode;
    emit searchModeChanged();
    runSearch();
}

void MainController::runSearch() {
    if (!m_index || m_searchQuery.isEmpty()) {
        m_searchModel.clear();
        return;
    }
    auto results = findByName(*m_index,
                              m_searchQuery.toStdString(),
                              static_cast<SearchMode>(m_searchMode));
    m_searchModel.setResults(std::move(results));
}

// ---------------------------------------------------------------------------
// Focus
// ---------------------------------------------------------------------------

void MainController::focusSymbol(qulonglong symbolId) {
    SymbolId sid = static_cast<SymbolId>(symbolId);
    if (sid == m_focusId) return;
    if (!m_index || !m_index->symbol(sid)) return;

    // Update history
    if (m_historyPos >= 0 &&
        m_historyPos < static_cast<int>(m_history.size()) - 1) {
        m_history.erase(m_history.begin() + m_historyPos + 1, m_history.end());
    }
    m_history.push_back(sid);
    if (static_cast<int>(m_history.size()) > MAX_HISTORY)
        m_history.erase(m_history.begin());
    m_historyPos = static_cast<int>(m_history.size()) - 1;

    m_focusId = sid;
    emit focusIdChanged();
    refreshEdgeLists();
}

void MainController::refreshEdgeLists() {
    auto ef = static_cast<EdgeKindMask>(m_edgeFilter);
    auto cf = static_cast<ConfidenceMask>(m_confidenceFilter);
    m_callersModel.loadEdges(m_focusId, /*callerMode=*/true,  ef, cf);
    m_calleesModel.loadEdges(m_focusId, /*callerMode=*/false, ef, cf);
}

QString MainController::focusQName() const {
    if (!m_index) return {};
    auto* s = m_index->symbol(m_focusId);
    return s ? QString::fromStdString(s->qualified_name) : QString();
}
QString MainController::focusFile() const {
    if (!m_index) return {};
    auto* s = m_index->symbol(m_focusId);
    return s && s->loc.valid() ? QString::fromStdString(s->loc.file) : QString();
}
int MainController::focusLine() const {
    if (!m_index) return 0;
    auto* s = m_index->symbol(m_focusId);
    return s ? s->loc.line : 0;
}
bool MainController::focusIsVirtual() const {
    if (!m_index) return false;
    auto* s = m_index->symbol(m_focusId);
    return s && s->is_virtual;
}
QString MainController::focusClass() const {
    if (!m_index) return {};
    auto* s = m_index->symbol(m_focusId);
    return s ? QString::fromStdString(s->class_qname) : QString();
}

int MainController::symbolCount() const {
    return m_index ? static_cast<int>(m_index->symbolCount()) : 0;
}
int MainController::edgeCount() const {
    return m_index ? static_cast<int>(m_index->edgeCount()) : 0;
}

// ---------------------------------------------------------------------------
// Filters
// ---------------------------------------------------------------------------

void MainController::setEdgeFilter(int f) {
    if (m_edgeFilter == f) return;
    m_edgeFilter = f;
    emit edgeFilterChanged();
    if (m_focusId != INVALID_SYMBOL_ID) refreshEdgeLists();
}

void MainController::setConfidenceFilter(int f) {
    if (m_confidenceFilter == f) return;
    m_confidenceFilter = f;
    emit confidenceFilterChanged();
    if (m_focusId != INVALID_SYMBOL_ID) refreshEdgeLists();
}

// ---------------------------------------------------------------------------
// Editor integration
// ---------------------------------------------------------------------------

void MainController::openInEditor(qulonglong symbolId) {
    if (!m_index) return;
    auto* s = m_index->symbol(static_cast<SymbolId>(symbolId));
    if (!s || !s->loc.valid()) return;

    QString scheme = m_settings.editorScheme();
    QString url = scheme
        .arg(QString::fromStdString(s->loc.file))
        .arg(s->loc.line);

    QDesktopServices::openUrl(QUrl(url));
}

// ---------------------------------------------------------------------------
// Export
// ---------------------------------------------------------------------------

void MainController::exportSlice(const QString& format, const QString& filePath) {
    if (!m_index || m_focusId == INVALID_SYMBOL_ID) {
        emit exportError("No symbol focused");
        return;
    }

    int depth = m_settings.exportDepth();
    auto ef = static_cast<EdgeKindMask>(m_edgeFilter);
    auto cf = static_cast<ConfidenceMask>(m_confidenceFilter);

    GraphSlice slice = neighboursOf(*m_index, m_focusId, depth, ef, cf);

    // Get git commit
    QString gitCommit = "unknown";
    {
        QProcess git;
        git.setWorkingDirectory(m_settings.sourceRoot());
        git.start("git", {"rev-parse", "--short", "HEAD"});
        if (git.waitForFinished(3000))
            gitCommit = QString::fromUtf8(git.readAllStandardOutput()).trimmed();
    }

    ExportHeader header;
    header.toolVersion = "1.0.0";
    header.sourceRoot  = m_settings.sourceRoot().toStdString();
    header.gitCommit   = gitCommit.toStdString();

    std::ofstream file(filePath.toStdString());
    if (!file.is_open()) {
        emit exportError("Cannot open file: " + filePath);
        return;
    }

    if (format == "mermaid") {
        MermaidExporter exp;
        exp.exportSlice(slice, *m_index, header, file);
    } else if (format == "dot") {
        DotExporter exp;
        exp.exportSlice(slice, *m_index, header, file);
    } else if (format == "drawio") {
        DrawioExporter exp;
        exp.exportSlice(slice, *m_index, header, file);
    } else if (format == "json") {
        JsonExporter exp;
        exp.exportSlice(slice, *m_index, header, file);
    } else {
        emit exportError("Unknown format: " + format);
        return;
    }

    emit exportDone(filePath);
    qCInfo(lcController) << "Exported" << format << "to" << filePath;
}

// ---------------------------------------------------------------------------
// Navigation
// ---------------------------------------------------------------------------

void MainController::navigateBack() {
    if (m_historyPos <= 0) return;
    --m_historyPos;
    m_focusId = m_history[static_cast<std::size_t>(m_historyPos)];
    emit focusIdChanged();
    refreshEdgeLists();
}

void MainController::navigateForward() {
    if (m_historyPos >= static_cast<int>(m_history.size()) - 1) return;
    ++m_historyPos;
    m_focusId = m_history[static_cast<std::size_t>(m_historyPos)];
    emit focusIdChanged();
    refreshEdgeLists();
}

bool MainController::canNavigateBack() const {
    return m_historyPos > 0;
}

bool MainController::canNavigateForward() const {
    return m_historyPos < static_cast<int>(m_history.size()) - 1;
}

} // namespace cg
