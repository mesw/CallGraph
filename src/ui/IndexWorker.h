#pragma once

#include "core/Index.h"
#include <QObject>
#include <QString>
#include <memory>

namespace cg {

// Runs discovery + parsing + resolution on a background thread.
// Usage: move to a QThread, call run() via QThread::started signal.
class IndexWorker : public QObject {
    Q_OBJECT

public:
    explicit IndexWorker(const QString& sourceRoot,
                         bool virtualExpansion,
                         QObject* parent = nullptr);

public slots:
    void run();

signals:
    void discoveryProgress(int filesFound);
    void parseProgress(int parsed, int total, int errors);
    // Emitted once on success
    void indexReady(std::shared_ptr<const cg::Index> index,
                    const QStringList& parseErrors);
    void finished(bool success, const QString& errorMessage);

private:
    QString m_sourceRoot;
    bool    m_virtualExpansion;
};

} // namespace cg
