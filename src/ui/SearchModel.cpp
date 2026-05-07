#include "ui/SearchModel.h"

namespace cg {

SearchModel::SearchModel(QObject* parent)
    : QAbstractListModel(parent)
{}

void SearchModel::setIndex(std::shared_ptr<const Index> index) {
    beginResetModel();
    m_index   = std::move(index);
    m_results.clear();
    endResetModel();
    emit countChanged();
}

void SearchModel::setResults(std::vector<SymbolId> results) {
    beginResetModel();
    m_results = std::move(results);
    endResetModel();
    emit countChanged();
}

void SearchModel::clear() {
    beginResetModel();
    m_results.clear();
    endResetModel();
    emit countChanged();
}

int SearchModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(m_results.size());
}

QVariant SearchModel::data(const QModelIndex& idx, int role) const {
    if (!idx.isValid() || !m_index) return {};
    int row = idx.row();
    if (row < 0 || row >= static_cast<int>(m_results.size())) return {};

    SymbolId sid = m_results[static_cast<std::size_t>(row)];
    const FunctionInfo* sym = m_index->symbol(sid);
    if (!sym) return {};

    switch (role) {
    case SymbolIdRole:       return static_cast<qulonglong>(sid);
    case QualifiedNameRole:  return QString::fromStdString(sym->qualified_name);
    case UnqualifiedNameRole:return QString::fromStdString(sym->unqualified_name);
    case ClassRole:          return QString::fromStdString(sym->class_qname);
    case ArityRole:          return sym->arity;
    case FileRole:           return QString::fromStdString(sym->loc.file);
    case LineRole:           return sym->loc.line;
    case IsVirtualRole:      return sym->is_virtual;
    case IsSyntheticRole:    return m_index->isSynthetic(sid);
    default:                 return {};
    }
}

QHash<int, QByteArray> SearchModel::roleNames() const {
    return {
        {SymbolIdRole,        "symbolId"},
        {QualifiedNameRole,   "qualifiedName"},
        {UnqualifiedNameRole, "unqualifiedName"},
        {ClassRole,           "className"},
        {ArityRole,           "arity"},
        {FileRole,            "file"},
        {LineRole,            "line"},
        {IsVirtualRole,       "isVirtual"},
        {IsSyntheticRole,     "isSynthetic"},
    };
}

qulonglong SearchModel::symbolIdAt(int row) const {
    if (row < 0 || row >= static_cast<int>(m_results.size())) return 0;
    return static_cast<qulonglong>(m_results[static_cast<std::size_t>(row)]);
}

} // namespace cg
