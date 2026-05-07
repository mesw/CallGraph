#include "ui/EdgeListModel.h"

namespace cg {

EdgeListModel::EdgeListModel(QObject* parent)
    : QAbstractListModel(parent)
{}

void EdgeListModel::setIndex(std::shared_ptr<const Index> index) {
    beginResetModel();
    m_index = std::move(index);
    m_rows.clear();
    endResetModel();
    emit countChanged();
}

void EdgeListModel::loadEdges(SymbolId focusId, bool callerMode,
                               EdgeKindMask edgeFilter,
                               ConfidenceMask confFilter) {
    beginResetModel();
    m_rows.clear();

    if (!m_index) {
        endResetModel();
        emit countChanged();
        return;
    }

    const auto& edgeIds = callerMode
        ? m_index->reverseEdges(focusId)
        : m_index->forwardEdges(focusId);

    for (EdgeId eid : edgeIds) {
        const Edge* e = m_index->edge(eid);
        if (!e) continue;
        if (!(edgeFilter & edgeBit(e->kind))) continue;
        if (!(confFilter & confidenceBit(e->confidence))) continue;

        Row row;
        row.edgeId     = eid;
        row.neighborId = callerMode ? e->from : e->to;
        row.callerMode = callerMode;
        m_rows.push_back(row);
    }

    endResetModel();
    emit countChanged();
}

void EdgeListModel::clear() {
    beginResetModel();
    m_rows.clear();
    endResetModel();
    emit countChanged();
}

int EdgeListModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(m_rows.size());
}

QVariant EdgeListModel::data(const QModelIndex& idx, int role) const {
    if (!idx.isValid() || !m_index) return {};
    int row = idx.row();
    if (row < 0 || row >= static_cast<int>(m_rows.size())) return {};

    const Row& r = m_rows[static_cast<std::size_t>(row)];
    const Edge* e = m_index->edge(r.edgeId);
    if (!e) return {};

    const FunctionInfo* nb = m_index->symbol(r.neighborId);

    switch (role) {
    case EdgeIdRole:
        return static_cast<qulonglong>(r.edgeId);
    case NeighborIdRole:
        return static_cast<qulonglong>(r.neighborId);
    case NeighborQNameRole:
        return nb ? QString::fromStdString(nb->qualified_name) : QString("?");
    case NeighborFileRole:
        return nb && nb->loc.valid() ? QString::fromStdString(nb->loc.file) : QString();
    case NeighborLineRole:
        return nb && nb->loc.valid() ? nb->loc.line : 0;
    case EdgeKindRole:
        return static_cast<int>(e->kind);
    case EdgeKindLabelRole:
        return QString(edgeKindLabel(e->kind));
    case ConfidenceRole:
        return static_cast<int>(e->confidence);
    case ConfidenceLabelRole:
        return QString(confidenceLabel(e->confidence));
    case ViaMacroRole:
        return e->via_macro ? QString::fromStdString(*e->via_macro) : QString();
    case IsResolvedRole:
        return e->kind != EdgeKind::Unresolved;
    default:
        return {};
    }
}

QHash<int, QByteArray> EdgeListModel::roleNames() const {
    return {
        {EdgeIdRole,         "edgeId"},
        {NeighborIdRole,     "neighborId"},
        {NeighborQNameRole,  "neighborQName"},
        {NeighborFileRole,   "neighborFile"},
        {NeighborLineRole,   "neighborLine"},
        {EdgeKindRole,       "edgeKind"},
        {EdgeKindLabelRole,  "edgeKindLabel"},
        {ConfidenceRole,     "confidence"},
        {ConfidenceLabelRole,"confidenceLabel"},
        {ViaMacroRole,       "viaMacro"},
        {IsResolvedRole,     "isResolved"},
    };
}

qulonglong EdgeListModel::neighborIdAt(int row) const {
    if (row < 0 || row >= static_cast<int>(m_rows.size())) return 0;
    return static_cast<qulonglong>(m_rows[static_cast<std::size_t>(row)].neighborId);
}

} // namespace cg
