#pragma once

#include "core/Index.h"
#include "core/Types.h"
#include <QAbstractListModel>
#include <memory>
#include <vector>

namespace cg {

class SearchModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        SymbolIdRole = Qt::UserRole + 1,
        QualifiedNameRole,
        UnqualifiedNameRole,
        ClassRole,
        ArityRole,
        FileRole,
        LineRole,
        IsVirtualRole,
        IsSyntheticRole
    };
    Q_ENUM(Roles)

    explicit SearchModel(QObject* parent = nullptr);

    void setIndex(std::shared_ptr<const Index> index);
    void setResults(std::vector<SymbolId> results);
    void clear();

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& idx, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE qulonglong symbolIdAt(int row) const;

signals:
    void countChanged();

private:
    std::shared_ptr<const Index> m_index;
    std::vector<SymbolId>        m_results;
};

} // namespace cg
