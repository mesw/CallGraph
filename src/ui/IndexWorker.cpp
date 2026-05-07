#include "ui/IndexWorker.h"

#include "discovery/FileDiscovery.h"
#include "parser/ParserPool.h"
#include "resolver/Resolver.h"

#include <QLoggingCategory>
#include <atomic>

Q_LOGGING_CATEGORY(lcWorker, "cg.worker")

namespace cg {

IndexWorker::IndexWorker(const QString& sourceRoot,
                         bool virtualExpansion,
                         QObject* parent)
    : QObject(parent)
    , m_sourceRoot(sourceRoot)
    , m_virtualExpansion(virtualExpansion)
{}

void IndexWorker::run() {
    qCInfo(lcWorker) << "IndexWorker: starting for" << m_sourceRoot;

    // --- Discovery ---
    DiscoveryOptions discOpts;
    discOpts.sourceRoot = m_sourceRoot.toStdString();

    auto discResult = discoverFiles(discOpts, [this](int found) {
        emit discoveryProgress(found);
    });

    if (!discResult.errorMessage.empty()) {
        emit finished(false, QString::fromStdString(discResult.errorMessage));
        return;
    }

    // --- Parsing ---
    FactBuffer facts;
    std::atomic<uint64_t> nextSymbolId{1};

    parseFiles(discResult.files, facts, nextSymbolId, [this](const ParserPoolProgress& p) {
        emit parseProgress(p.filesParsed, p.totalFiles, p.parseErrors);
    });

    // --- Resolution ---
    ResolverOptions resOpts;
    resOpts.enableVirtualExpansion = m_virtualExpansion;

    Resolver resolver(resOpts);
    auto index = resolver.resolve(facts);

    QStringList errorList;
    {
        std::lock_guard<std::mutex> lk(facts.mtx);
        for (const auto& err : facts.parseErrors)
            errorList.append(QString::fromStdString(err));
    }

    qCInfo(lcWorker) << "IndexWorker: done."
                     << index->symbolCount() << "symbols,"
                     << index->edgeCount() << "edges";

    emit indexReady(std::shared_ptr<const Index>(std::move(index)), errorList);
    emit finished(true, {});
}

} // namespace cg
