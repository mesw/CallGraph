# CLAUDE.md

This file is the entry point for Claude Code working in this repository. Read it first, every session.

## Project: CallGraph

A desktop GUI tool that builds and visualises call graphs from a large legacy **C++98** codebase. The codebase being analysed is *not* this project — this project is the analyser.

**Primary use case:** given a function name, find every caller and callee, including indirect uses (function pointer assignments, calls inside macros, virtual override candidates), and export the resulting subgraph as Mermaid, Graphviz DOT, draw.io XML, JSON, or SVG/PNG.

## Versioning

The project ships in phases. Each phase is independently usable.

| Version | Scope | Status |
|---|---|---|
| **V1** | Indexer + search + details panes + all export formats. **No in-app graph view.** | current target |
| **V2a** | V1 + in-app graph view using Graphviz `dot.exe` for layout (hierarchical, directional). | future |
| **V2b** | V1 + in-app graph view using an in-process force-directed physics layout. | future, alternative to V2a |

**Always check ARCHITECTURE.md §Roadmap before starting work** — it specifies which directories, classes, and dependencies belong to which version.

V2a and V2b are alternatives, not sequential. Pick one based on user feedback after V1 ships. They could later coexist as a user-selectable layout option, but do not implement both speculatively.

## Tech stack

- **Language:** C++17 (the *tool* is modern C++; the *input* it parses is C++98)
- **GUI:** Qt 6.8.3 + Qt Quick / QML
- **Parser:** [tree-sitter](https://tree-sitter.github.io/) with the `tree-sitter-cpp` grammar, vendored as a git submodule under `third_party/`
- **Build:** CMake ≥ 3.21, MSVC on Windows, static linking
- **Target:** Windows dev box only. Do **not** add Linux/macOS/WASM targets without being asked.
- **No CI.** Local builds only.

Version-specific dependencies:

- **V1**: Qt, tree-sitter, tree-sitter-cpp.
- **V2a**: V1 dependencies + `dot.exe` (Graphviz, vendored next to the executable).
- **V2b**: V1 dependencies only. No external layout binary.

## Build and run

Two PowerShell scripts at the repo root, mirroring the convention used in Michael's other Qt projects (separate per-platform scripts, no unified entry point):

```powershell
.\build-windows.ps1        # configures + builds Release
.\run-windows.ps1          # launches the app pointing at $env:CALLGRAPH_SOURCE_ROOT
```

`CMakePresets.json` defines `windows-msvc-release` and `windows-msvc-debug`. No other presets.

## Repository layout

```
.
├── CLAUDE.md                  <- this file
├── ARCHITECTURE.md            <- detailed component design, read before non-trivial changes
├── REQUIREMENTS.md            <- functional + quality requirements, source of truth for "what"
├── CMakeLists.txt
├── CMakePresets.json
├── build-windows.ps1
├── run-windows.ps1
├── src/
│   ├── core/                  <- V1: Index, types, edge taxonomy (no Qt deps below this layer)
│   ├── discovery/             <- V1: git ls-files walker
│   ├── parser/                <- V1: tree-sitter wrapper, fact extractor
│   ├── resolver/              <- V1: name resolution, virtual override expansion
│   ├── query/                 <- V1: BFS engines, graph slicing
│   ├── export/                <- V1: Mermaid, DOT, draw.io, JSON, SVG/PNG
│   ├── ui/                    <- V1: C++ side of QML bindings (controllers, models)
│   ├── qml/                   <- V1: QML files (search + details panes only)
│   ├── graph_dot/             <- V2a only: Graphviz layout integration
│   └── graph_physics/         <- V2b only: force-directed layout engine
├── third_party/
│   ├── tree-sitter/           <- V1
│   └── tree-sitter-cpp/       <- V1
├── vendor/
│   └── graphviz/dot.exe       <- V2a only
└── tests/
```

The `core/` layer must not depend on Qt. Everything above it may. This keeps the index, parser, and exporters trivially unit-testable without a `QGuiApplication`.

## Hard rules

1. **No persistence.** The index is rebuilt from scratch every session. No SQLite, no LMDB, no on-disk caching. Branch-scoped in-memory only.
2. **No compilation database.** Do not try to read `.sln`, `.vcxproj`, or generate `compile_commands.json`. File discovery is `git ls-files` + extension filter, full stop. The legacy build is MSVC-only, projects reference folders, and not all compiled files appear in solutions.
3. **No libclang.** We rejected it for V1 — the C++98 codebase is preprocessor-heavy and we cannot synthesise correct include paths. Tree-sitter only.
4. **No Qt below `src/ui/`, `src/qml/`, `src/graph_dot/`, `src/graph_physics/`.** The core, parser, resolver, query, and export layers are pure C++17 + STL + tree-sitter. Enforced by CMake target link rules.
5. **Static linking.** Qt and tree-sitter both linked statically into the final exe. No DLL deployment.
6. **C++17, not later.** Even though Qt 6.8 supports C++20, stay on 17 for predictability.
7. **No graph view in V1.** Don't speculatively scaffold a graph component. The V1 UX is search + details panes only; visual graphs come exclusively via exports.
8. **V2a and V2b are alternatives.** Don't implement both at once.

## Code conventions

- **Naming:** `PascalCase` for types, `camelCase` for functions and variables, `m_` prefix for non-public class members, `SCREAMING_SNAKE_CASE` for constants and macros.
- **Headers:** `#pragma once`, no include guards.
- **Includes:** project headers in `""`, third-party and STL in `<>`. Group: project, third-party, Qt, STL — in that order.
- **Strings:** `std::string` and `std::string_view` in core/parser/resolver/query/export. `QString` only at the QML boundary in `src/ui/`. Convert at the boundary, not deeper.
- **Containers:** STL (`std::vector`, `std::unordered_map`) in core. `QAbstractListModel` subclasses for QML-exposed lists.
- **Errors:** exceptions for programmer errors and unrecoverable parse failures; `std::optional`-style return values for "user input was bad" cases. Never silently swallow.
- **Threading:** parsing is parallel (`QThreadPool`); resolver is single-threaded; query engine is read-only against an immutable index, no locking. If you add mutation post-build, stop and reconsider — the immutability is load-bearing.
- **Logging:** `qDebug` / `qInfo` / `qWarning` / `qCritical` with categorised logging (`Q_LOGGING_CATEGORY` per layer).

## What lives where

| Need to change... | Edit in... | Read first |
|---|---|---|
| File-type filter, git walking | `src/discovery/` | ARCHITECTURE.md §Discovery |
| Tree-sitter queries, fact extraction | `src/parser/` | ARCHITECTURE.md §Parser, §Fact extractor |
| Overload handling, virtual expansion, macro edges | `src/resolver/` | ARCHITECTURE.md §Resolver |
| BFS, depth limits, edge filters | `src/query/` | ARCHITECTURE.md §Query engine |
| Add a new export format | `src/export/` | ARCHITECTURE.md §Export |
| New UI panel, search box, details view | `src/ui/` and `src/qml/` | ARCHITECTURE.md §UI |
| **V2a only** — Graphviz layout, DOT-based view | `src/graph_dot/` | ARCHITECTURE.md §V2a graph view |
| **V2b only** — physics simulation, force-directed view | `src/graph_physics/` | ARCHITECTURE.md §V2b graph view |

## Common pitfalls

- **Don't use `std::regex`** in the parser layer. Use tree-sitter queries. Regex is for the discovery layer's filename filtering only.
- **Don't try to disambiguate overloads by argument type.** Match on `qualified_name + arity` only. If a call resolves to multiple definitions, store *all* candidates as edges with `confidence=overload`. The user has explicitly accepted this fuzziness.
- **Don't try to handle token-pasting macros** (`Do##x()`). Mark the call site as `Unresolved` with the macro body in the tooltip and move on.
- **Don't reach for libclang** when something is hard to extract. The answer is "more tree-sitter queries" or "live with the imprecision and label the edge appropriately."
- **Don't add a database.** If you find yourself wanting one, you've misunderstood the lifecycle: build → query → exit.
- **In V1, don't render the graph in QML.** The `src/qml/` files are a search pane and details panes only. If you find yourself drawing nodes and edges, you've crossed into V2.
- **In V2b, don't use Qt's animation framework for physics.** The simulation runs on `QQuickItem::updatePolish()` driven by `QQuickWindow::frameSwapped`. See ARCHITECTURE.md §V2b.

## Definition of done for any change

1. Builds clean under `windows-msvc-release` with `/W4 /WX`.
2. Unit tests for `core/`, `parser/`, `resolver/`, `query/`, `export/` pass. UI is not unit-tested; manual smoke test.
3. ARCHITECTURE.md updated if a layer's responsibility, inputs, or outputs changed.
4. REQUIREMENTS.md updated if functional scope changed.
5. No new dependencies without an entry in ARCHITECTURE.md §Dependencies.

## Pointers

- **What to build (per version):** REQUIREMENTS.md
- **How it's structured (per version):** ARCHITECTURE.md
- **Tree-sitter C++ grammar reference:** `third_party/tree-sitter-cpp/src/grammar.json`
- **Qt 6.8.3 docs:** https://doc.qt.io/qt-6.8/
