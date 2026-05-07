#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace cg {

struct DiscoveryOptions {
    std::string              sourceRoot;
    std::vector<std::string> includePathPrefixes; // empty = include all
    std::vector<std::string> excludePathPrefixes;
    // Default patterns to skip generated/build directories
    std::vector<std::string> excludePatterns = { "/generated/", "/build/", "/_build/" };
};

struct DiscoveryResult {
    std::vector<std::filesystem::path> files;
    std::string errorMessage; // non-empty if git invocation failed
};

// Progress callback: (filesFound so far)
using DiscoveryProgressFn = std::function<void(int)>;

DiscoveryResult discoverFiles(const DiscoveryOptions& opts,
                              DiscoveryProgressFn progress = nullptr);

} // namespace cg
