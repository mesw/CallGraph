#include "parser/ParserPool.h"
#include "parser/TreeSitterParser.h"
#include "parser/FactExtractor.h"

#include <QFile>
#include <QRunnable>
#include <QThread>
#include <QThreadPool>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcParser, "cg.parser")

namespace cg {

// ---------------------------------------------------------------------------
// Per-file parse task
// ---------------------------------------------------------------------------

class ParseTask : public QRunnable {
public:
    ParseTask(std::filesystem::path path,
              FactBuffer& buffer,
              std::atomic<uint64_t>& nextSymbolId,
              std::atomic<int>& doneCount,
              std::atomic<int>& hardErrorCount,
              std::atomic<int>& softErrorCount)
        : m_path(std::move(path))
        , m_buffer(buffer)
        , m_nextSymbolId(nextSymbolId)
        , m_doneCount(doneCount)
        , m_hardErrorCount(hardErrorCount)
        , m_softErrorCount(softErrorCount)
    {
        setAutoDelete(true);
    }

    void run() override {
        QFile file(QString::fromStdString(m_path.string()));
        if (!file.open(QIODevice::ReadOnly)) {
            qCWarning(lcParser) << "Cannot open:" << QString::fromStdString(m_path.string());
            m_buffer.appendParseError(m_path.string());
            ++m_hardErrorCount;
            ++m_doneCount;
            return;
        }

        qint64 size = file.size();
        if (size == 0) { ++m_doneCount; return; }

        const uchar* mapped = file.map(0, size);
        if (!mapped) {
            qCWarning(lcParser) << "Cannot map:" << QString::fromStdString(m_path.string());
            m_buffer.appendParseError(m_path.string());
            ++m_hardErrorCount;
            ++m_doneCount;
            return;
        }

        TreeSitterParser parser;
        bool ok = parser.parse(reinterpret_cast<const char*>(mapped),
                               static_cast<std::size_t>(size));
        if (!ok || !parser.tree()) {
            qCWarning(lcParser) << "Parse failed:" << QString::fromStdString(m_path.string());
            m_buffer.appendParseError(m_path.string());
            ++m_hardErrorCount;
            ++m_doneCount;
            file.unmap(const_cast<uchar*>(mapped));
            return;
        }

        // Soft error: tree-sitter recovered but flagged ERROR nodes.
        // We still extract — results will be approximate for this file.
        if (parser.hasErrors())
            ++m_softErrorCount;

        FactExtractor extractor(m_buffer, m_nextSymbolId);
        extractor.extractFromFile(m_path,
                                  reinterpret_cast<const char*>(mapped),
                                  static_cast<std::size_t>(size),
                                  parser.tree());

        file.unmap(const_cast<uchar*>(mapped));
        ++m_doneCount;
    }

private:
    std::filesystem::path   m_path;
    FactBuffer&             m_buffer;
    std::atomic<uint64_t>&  m_nextSymbolId;
    std::atomic<int>&       m_doneCount;
    std::atomic<int>&       m_hardErrorCount;
    std::atomic<int>&       m_softErrorCount;
};

// ---------------------------------------------------------------------------
// Parse coordinator
// ---------------------------------------------------------------------------

void parseFiles(const std::vector<std::filesystem::path>& files,
                FactBuffer& buffer,
                std::atomic<uint64_t>& nextSymbolId,
                ParserProgressFn progress) {
    int total = static_cast<int>(files.size());
    if (total == 0) return;

    std::atomic<int> doneCount{0};
    std::atomic<int> hardErrorCount{0};
    std::atomic<int> softErrorCount{0};

    QThreadPool pool;
    pool.setMaxThreadCount(static_cast<int>(QThread::idealThreadCount()));

    for (const auto& path : files) {
        pool.start(new ParseTask(path, buffer, nextSymbolId,
                                 doneCount, hardErrorCount, softErrorCount));
    }

    auto makeProgress = [&]() {
        ParserPoolProgress prog;
        prog.filesParsed = doneCount.load();
        prog.totalFiles  = total;
        prog.hardErrors  = hardErrorCount.load();
        prog.softErrors  = softErrorCount.load();
        return prog;
    };

    while (!pool.waitForDone(100)) {
        if (progress) progress(makeProgress());
    }
    if (progress) progress(makeProgress());

    qCInfo(lcParser) << "Parsed" << doneCount.load() << "/" << total
                     << "files —" << hardErrorCount.load() << "hard errors,"
                     << softErrorCount.load() << "with partial tree-sitter errors";
}

} // namespace cg
