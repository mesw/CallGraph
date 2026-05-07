#pragma once

#include "core/Index.h"
#include "core/Types.h"
#include "ui/AppSettings.h"
#include "ui/EdgeListModel.h"
#include "ui/SearchModel.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <memory>
#include <vector>

namespace cg {

class MainController : public QObject {
    Q_OBJECT

    // Index state
    Q_PROPERTY(bool indexReady READ indexReady NOTIFY indexReadyChanged)
    Q_PROPERTY(bool indexBuilding READ indexBuilding NOTIFY indexBuildingChanged)
    Q_PROPERTY(QString indexStatus READ indexStatus NOTIFY indexStatusChanged)
    Q_PROPERTY(int symbolCount READ symbolCount NOTIFY symbolCountChanged)
    Q_PROPERTY(int edgeCount READ edgeCount NOTIFY edgeCountChanged)
    Q_PROPERTY(QStringList parseErrors READ parseErrors NOTIFY parseErrorsChanged)

    // Search
    Q_PROPERTY(QString searchQuery READ searchQuery WRITE setSearchQuery NOTIFY searchQueryChanged)
    Q_PROPERTY(int searchMode READ searchMode WRITE setSearchMode NOTIFY searchModeChanged)

    // Focus
    Q_PROPERTY(qulonglong focusId READ focusId NOTIFY focusIdChanged)
    Q_PROPERTY(QString focusQName READ focusQName NOTIFY focusIdChanged)
    Q_PROPERTY(QString focusFile READ focusFile NOTIFY focusIdChanged)
    Q_PROPERTY(int focusLine READ focusLine NOTIFY focusIdChanged)
    Q_PROPERTY(bool focusIsVirtual READ focusIsVirtual NOTIFY focusIdChanged)
    Q_PROPERTY(QString focusClass READ focusClass NOTIFY focusIdChanged)

    // Filter
    Q_PROPERTY(int edgeFilter READ edgeFilter WRITE setEdgeFilter NOTIFY edgeFilterChanged)
    Q_PROPERTY(int confidenceFilter READ confidenceFilter WRITE setConfidenceFilter NOTIFY confidenceFilterChanged)

    // Models (exposed to QML)
    Q_PROPERTY(cg::SearchModel*   searchModel   READ searchModel   CONSTANT)
    Q_PROPERTY(cg::EdgeListModel* callersModel  READ callersModel  CONSTANT)
    Q_PROPERTY(cg::EdgeListModel* calleesModel  READ calleesModel  CONSTANT)
    Q_PROPERTY(cg::AppSettings*   settings      READ settings      CONSTANT)

public:
    explicit MainController(QObject* parent = nullptr);

    // Index state
    bool      indexReady()    const { return m_indexReady; }
    bool      indexBuilding() const { return m_indexBuilding; }
    QString   indexStatus()   const { return m_indexStatus; }
    int       symbolCount()   const;
    int       edgeCount()     const;
    QStringList parseErrors() const { return m_parseErrors; }

    // Search
    QString searchQuery() const { return m_searchQuery; }
    void    setSearchQuery(const QString& q);
    int     searchMode() const { return m_searchMode; }
    void    setSearchMode(int mode);

    // Focus
    qulonglong focusId()       const { return static_cast<qulonglong>(m_focusId); }
    QString    focusQName()    const;
    QString    focusFile()     const;
    int        focusLine()     const;
    bool       focusIsVirtual()const;
    QString    focusClass()    const;

    // Filter
    int edgeFilter()       const { return m_edgeFilter; }
    void setEdgeFilter(int f);
    int confidenceFilter() const { return m_confidenceFilter; }
    void setConfidenceFilter(int f);

    // Models
    SearchModel*   searchModel()  { return &m_searchModel; }
    EdgeListModel* callersModel() { return &m_callersModel; }
    EdgeListModel* calleesModel() { return &m_calleesModel; }
    AppSettings*   settings()     { return &m_settings; }

public slots:
    Q_INVOKABLE void buildIndex();
    Q_INVOKABLE void focusSymbol(qulonglong symbolId);
    Q_INVOKABLE void openInEditor(qulonglong symbolId);
    Q_INVOKABLE void exportSlice(const QString& format, const QString& filePath);
    Q_INVOKABLE void navigateBack();
    Q_INVOKABLE void navigateForward();
    Q_INVOKABLE bool canNavigateBack()    const;
    Q_INVOKABLE bool canNavigateForward() const;

signals:
    void indexReadyChanged();
    void indexBuildingChanged();
    void indexStatusChanged();
    void symbolCountChanged();
    void edgeCountChanged();
    void parseErrorsChanged();
    void searchQueryChanged();
    void searchModeChanged();
    void focusIdChanged();
    void edgeFilterChanged();
    void confidenceFilterChanged();
    void exportError(const QString& message);
    void exportDone(const QString& filePath);

private:
    void onIndexReady(std::shared_ptr<const Index> index, const QStringList& errors);
    void onIndexFinished(bool success, const QString& errorMessage);
    void refreshEdgeLists();
    void runSearch();

    AppSettings   m_settings;
    SearchModel   m_searchModel;
    EdgeListModel m_callersModel;
    EdgeListModel m_calleesModel;

    std::shared_ptr<const Index> m_index;
    bool    m_indexReady    = false;
    bool    m_indexBuilding = false;
    QString m_indexStatus;
    QStringList m_parseErrors;

    QString m_searchQuery;
    int     m_searchMode  = 0; // SearchMode::Substring

    SymbolId m_focusId = INVALID_SYMBOL_ID;

    int m_edgeFilter       = static_cast<int>(ALL_EDGE_KINDS);
    int m_confidenceFilter = static_cast<int>(ALL_CONFIDENCE);

    // Navigation history
    std::vector<SymbolId> m_history;
    int                   m_historyPos = -1;
};

} // namespace cg
