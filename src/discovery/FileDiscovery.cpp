#include "discovery/FileDiscovery.h"

#include <QProcess>
#include <QDir>
#include <QLoggingCategory>
#include <algorithm>
#include <sstream>

Q_LOGGING_CATEGORY(lcDiscovery, "cg.discovery")

namespace cg {

static const std::vector<std::string> CPP_EXTENSIONS = {
    ".h", ".hpp", ".hxx", ".inl", ".c", ".cpp", ".cxx", ".cc", ".ipp"
};

static bool hasCppExtension(const std::string& path) {
    auto dot = path.rfind('.');
    if (dot == std::string::npos) return false;
    std::string ext = path.substr(dot);
    for (const auto& e : CPP_EXTENSIONS) {
        if (ext == e) return true;
    }
    return false;
}

static bool matchesAnyPrefix(const std::string& path,
                              const std::vector<std::string>& prefixes) {
    for (const auto& prefix : prefixes) {
        if (path.find(prefix) != std::string::npos) return true;
    }
    return false;
}

DiscoveryResult discoverFiles(const DiscoveryOptions& opts,
                              DiscoveryProgressFn progress) {
    DiscoveryResult result;

    QProcess git;
    git.setWorkingDirectory(QString::fromStdString(opts.sourceRoot));
    git.start("git", {"ls-files", "-z", "--full-name"});

    if (!git.waitForStarted(5000)) {
        result.errorMessage = "Failed to start git: " + git.errorString().toStdString();
        qCCritical(lcDiscovery) << QString::fromStdString(result.errorMessage);
        return result;
    }

    if (!git.waitForFinished(120000)) { // 2 minute timeout
        git.kill();
        result.errorMessage = "git ls-files timed out";
        qCCritical(lcDiscovery) << QString::fromStdString(result.errorMessage);
        return result;
    }

    if (git.exitCode() != 0) {
        result.errorMessage = "git ls-files failed: "
            + git.readAllStandardError().toStdString();
        qCCritical(lcDiscovery) << QString::fromStdString(result.errorMessage);
        return result;
    }

    QByteArray output = git.readAllStandardOutput();
    const char* data  = output.constData();
    const char* end   = data + output.size();

    std::string root = opts.sourceRoot;
    if (!root.empty() && root.back() != '/' && root.back() != '\\')
        root += '/';

    int found = 0;
    const char* cur = data;
    while (cur < end) {
        const char* nul = cur;
        while (nul < end && *nul != '\0') ++nul;

        std::string relPath(cur, nul);
        cur = nul + 1;

        if (!hasCppExtension(relPath)) continue;

        // Exclude filters
        if (!opts.excludePatterns.empty() &&
            matchesAnyPrefix(relPath, opts.excludePatterns)) continue;

        if (!opts.excludePathPrefixes.empty() &&
            matchesAnyPrefix(relPath, opts.excludePathPrefixes)) continue;

        // Include filter
        if (!opts.includePathPrefixes.empty() &&
            !matchesAnyPrefix(relPath, opts.includePathPrefixes)) continue;

        std::filesystem::path absPath = std::filesystem::path(root) / relPath;
        result.files.push_back(std::move(absPath));
        ++found;

        if (progress && (found % 500 == 0))
            progress(found);
    }

    if (progress) progress(found);

    qCInfo(lcDiscovery) << "Discovered" << found << "files under" <<
        QString::fromStdString(opts.sourceRoot);
    return result;
}

} // namespace cg
