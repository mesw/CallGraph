#pragma once

#include "core/Index.h"
#include "core/Types.h"
#include <QAbstractListModel>
#include <memory>
#include <vector>

namespace cg {

// Displays a list of edges (either callers or callees) for the focused symbol.
class EdgeListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        EdgeIdRole = Qt::UserRole + 1,
        NeighborIdRole,        // the "other" end of the edge (caller or callee)
        NeighborQNameRole,
        NeighborFileRole,
        NeighborLineRole,
        EdgeKindRole,
        EdgeKindLabelRole,
        ConfidenceRole,
        ConfidenceLabelRole,
        ViaMacroRole,
        IsResolvedRole
    };
    Q_ENUM(Roles)

    explicit EdgeListModel(QObject* parent = nullptr);

    void setIndex(std::shared_ptr<const Index> index);

    // mode: true = show callers (reverse edges), false = show callees (forward edges)
    void loadEdges(SymbolId focusId, bool callerMode,
                   EdgeKindMask edgeFilter   = ALL_EDGE_KINDS,
                   ConfidenceMask confFilter = ALL_CONFIDENCE);
    void clear();

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& idx, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE qulonglong neighborIdAt(int row) const;

signals:
    void countChanged();

private:
    struct Row {
        EdgeId   edgeId;
        SymbolId neighborId;
        bool     callerMode; // true → row.neighborId is the caller
    };

    std::shared_ptr<const Index> m_index;
    std::vector<Row>             m_rows;
};

} // namespace cg
