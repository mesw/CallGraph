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
              std::atomic<int>& errorCount)
        : m_path(std::move(path))
        , m_buffer(buffer)
        , m_nextSymbolId(nextSymbolId)
        , m_doneCount(doneCount)
        , m_errorCount(errorCount)
    {
        setAutoDelete(true);
    }

    void run() override {
        // Memory-map the file
        QFile file(QString::fromStdString(m_path.string()));
        if (!file.open(QIODevice::ReadOnly)) {
            qCWarning(lcParser) << "Cannot open:" << QString::fromStdString(m_path.string());
            m_buffer.appendParseError(m_path.string());
            ++m_errorCount;
            ++m_doneCount;
            return;
        }

        qint64 size = file.size();
        if (size == 0) {
            ++m_doneCount;
            return;
        }

        const uchar* mapped = file.map(0, size);
        if (!mapped) {
            qCWarning(lcParser) << "Cannot map:" << QString::fromStdString(m_path.string());
            m_buffer.appendParseError(m_path.string());
            ++m_errorCount;
            ++m_doneCount;
            return;
        }

        TreeSitterParser parser;
        bool ok = parser.parse(reinterpret_cast<const char*>(mapped),
                               static_cast<std::size_t>(size));
        if (!ok || !parser.tree()) {
            qCWarning(lcParser) << "Parse failed:" << QString::fromStdString(m_path.string());
            m_buffer.appendParseError(m_path.string());
            ++m_errorCount;
            ++m_doneCount;
            file.unmap(const_cast<uchar*>(mapped));
            return;
        }

        if (parser.hasErrors()) {
            qCWarning(lcParser) << "Parse errors in:" << QString::fromStdString(m_path.string());
            // Do not append to parseErrors — we still extract what we can
            ++m_errorCount;
        }

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
    std::atomic<int>&       m_errorCount;
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
    std::atomic<int> errorCount{0};

    QThreadPool pool;
    pool.setMaxThreadCount(static_cast<int>(QThread::idealThreadCount()));

    for (const auto& path : files) {
        pool.start(new ParseTask(path, buffer, nextSymbolId, doneCount, errorCount));
    }

    // Progress reporting loop — 100 ms intervals
    while (!pool.waitForDone(100)) {
        if (progress) {
            ParserPoolProgress prog;
            prog.filesParsed = doneCount.load();
            prog.totalFiles  = total;
            prog.parseErrors = errorCount.load();
            progress(prog);
        }
    }

    if (progress) {
        ParserPoolProgress prog;
        prog.filesParsed = doneCount.load();
        prog.totalFiles  = total;
        prog.parseErrors = errorCount.load();
        progress(prog);
    }

    qCInfo(lcParser) << "Parsed" << doneCount.load() << "/" << total
                     << "files," << errorCount.load() << "with errors";
}

} // namespace cg
