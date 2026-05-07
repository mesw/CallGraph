# REQUIREMENTS.md

Source of truth for *what* CallGraph must do. ARCHITECTURE.md is the source of truth for *how*.

## Version legend

Each requirement is tagged with the earliest version that must satisfy it.

- **[V1]** — must be in the first ship. CLI-feel UI: search + details panes + exports. No in-app graph view.
- **[V2a]** — adds an in-app graph view backed by Graphviz `dot.exe`.
- **[V2b]** — adds an in-app graph view backed by an in-process force-directed physics layout. Alternative to V2a.

V2a and V2b are alternatives, not stages. Pick one after V1 ships based on user feedback.

## Purpose

Replace the current manual workflow of `grep -rE` + manual classification + manual Mermaid/draw.io graph drafting with a tool that:

1. Indexes a C++98 codebase from a git working tree.
2. Lets the user search for a function and see all callers, callees, and indirect uses.
3. Exports the resulting subgraph in formats already used downstream (Mermaid, draw.io) and adds a few more.
4. *(V2)* Provides an interactive in-app graph view for exploration.

## Functional requirements

### FR-1 Source discovery [V1]
- **FR-1.1** The user provides an absolute path to a directory inside a git working tree.
- **FR-1.2** The tool discovers C/C++ source files via `git ls-files` filtered to extensions: `.h .hpp .hxx .inl .c .cpp .cxx .cc .ipp`.
- **FR-1.3** The tool must respect the currently checked-out branch. Switching branches in git outside the tool requires a re-index.
- **FR-1.4** Optional include/exclude path-prefix filters allow the user to scope indexing to a subdirectory.

### FR-2 Indexing [V1]
- **FR-2.1** The tool parses every discovered file with tree-sitter using the C++ grammar.
- **FR-2.2** The tool must extract:
  - function and method definitions with qualified name, arity, source location
  - call sites with caller, callee identifier, arity, source location
  - function pointer assignments (`fp = &func;` and `register(&func)`)
  - calls to known thread-spawn APIs (configurable list)
  - macro definitions and call tokens within them
  - class declarations with base classes
  - virtual / static markers on methods (best-effort under C++98 syntax)
- **FR-2.3** Indexing must complete in finite time and report progress (files done / total) at least every second.
- **FR-2.4** Files that fail to parse must produce a warning with the file path; indexing must continue.

### FR-3 Resolution [V1]
- **FR-3.1** Calls resolve on `qualified_name + arity`. Argument types are **not** considered.
- **FR-3.2** When a name resolves to multiple definitions (overloads), the tool emits an edge to **every** candidate, marked `confidence=Overload`. The UI must surface the candidate count.
- **FR-3.3** Calls to identifiers not present in the index produce edges to a synthetic "Unresolved" node carrying the original identifier.
- **FR-3.4** A user-toggleable mode adds `VirtualCandidate` edges from virtual method calls to all overrides in the class hierarchy. Off by default.
- **FR-3.5** Calls observed inside macro expansions are annotated with the originating macro name, and the macro itself becomes a node in the graph.

### FR-4 Search [V1]
- **FR-4.1** A search box accepts substring, exact-qualified-name, and regex queries.
- **FR-4.2** Results show qualified name, signature (params as written), file, and line.
- **FR-4.3** Selecting a result focuses it.

### FR-5 Details panes [V1]
- **FR-5.1** A focus pane shows the selected symbol's qualified name, full signature, and source location.
- **FR-5.2** A "open in editor" action opens the source location in the user's configured editor (configurable URI scheme: `vscode://`, `idea://`, etc.).
- **FR-5.3** A callers pane lists every edge in `m_reverse[focus]`. Columns: caller name, edge kind icon, confidence icon, file:line.
- **FR-5.4** A callees pane lists every edge in `m_forward[focus]`. Same columns.
- **FR-5.5** Clicking a row in either list focuses that symbol (back/forward navigation).
- **FR-5.6** The detail panes update with the focused symbol; depth is implicit (1 level in each direction).

### FR-6 Filtering [V1]
- **FR-6.1** The user can include/exclude edges by `EdgeKind`.
- **FR-6.2** The user can include/exclude edges by `Confidence`.
- **FR-6.3** Filters apply to the details panes and to exports. (And to the graph view in V2.)

### FR-7 Export [V1, except as noted]
- **FR-7.1** The current graph slice (post-filter) can be exported as:
  - Mermaid (`flowchart TD`) **[V1]**
  - Graphviz DOT **[V1]**
  - draw.io / diagrams.net XML **[V1]**
  - JSON (documented schema) **[V1]**
  - SVG **[V2; deferred from V1 — graph view's laid-out scene is the source]**
  - PNG **[V2]**
- **FR-7.2** Export preserves edge-kind styling consistently across Mermaid / DOT / draw.io.
- **FR-7.3** Exports include a header comment with: tool version, source root, git commit, query parameters (root symbol, depth, filters).
- **FR-7.4** The exported slice is `callersOf(focus, depth) ∪ calleesOf(focus, depth)` where `depth` is set in the export dialog.

### FR-8 Session lifecycle [V1]
- **FR-8.1** The tool builds the index on demand at startup or when the user picks a new source root.
- **FR-8.2** No persistence: closing the tool discards the index. Reopening rebuilds.
- **FR-8.3** The user can trigger a "rebuild index" action explicitly (e.g. after switching git branches).

### FR-9 Graph view [V2a / V2b]
- **FR-9.1** **[V2a/V2b]** The graph view displays nodes and edges of the current slice.
- **FR-9.2** **[V2a/V2b]** The default depth is 2 in each direction, configurable up to 10.
- **FR-9.3** **[V2a/V2b]** Boundary nodes (those reaching the depth limit but having further neighbours) are visually marked and clickable to expand.
- **FR-9.4** **[V2a/V2b]** Edge kind is visually distinguishable (line style + colour, see ARCHITECTURE.md).
- **FR-9.5** **[V2a/V2b]** Confidence (Exact / Overload / Heuristic) is visually distinguishable.
- **FR-9.6** **[V2a/V2b]** Selecting a node in the graph updates the details panes.
- **FR-9.7** **[V2a/V2b]** The user can pin a node so re-running queries from elsewhere keeps it in view (within a slice).
- **FR-9.8** **[V2a]** Layout is computed by `dot.exe -Tjson` invoked as a subprocess. Layout is static per slice; click-to-expand re-runs layout.
- **FR-9.9** **[V2b]** Layout is computed by an in-process force-directed simulation. Nodes are draggable; the simulation responds in real time.
- **FR-9.10** **[V2b]** A "freeze layout" toggle pauses the simulation, allowing the user to settle on a layout for export.
- **FR-9.11** **[V2b]** Direction of flow (callers above, callees below) is visually preserved via directional gravity in the force model. Disabling directional gravity is a developer-mode option, not a user-facing one.

## Quality (non-functional) requirements

### QR-1 Speed of lookup *(stated user priority)*
- **QR-1.1** **[V1]** Name search returns first results within **100 ms** for any query against an index of up to 1M symbols.
- **QR-1.2** **[V1]** Callers/callees query (depth 2) returns within **50 ms** for any symbol.
- **QR-1.3** **[V2a]** Graph layout via Graphviz for slices up to 200 nodes returns within **2 s**.
- **QR-1.4** **[V2b]** Force-directed simulation maintains **60 fps** for slices up to 300 nodes (naïve), and up to 5000 nodes with Barnes–Hut.
- **QR-1.5** **[V1]** Initial indexing of a 5 MLOC codebase completes within **5 minutes** on a current Windows dev box (~16 cores, NVMe SSD). Stretch target.

### QR-2 Completeness of results *(stated user priority)*
- **QR-2.1** **[V1]** Every call syntactically present in parsed source must produce an edge — either resolved, overload-resolved, or pointing to a synthetic Unresolved node. No call is silently dropped.
- **QR-2.2** **[V1]** Every function pointer assignment to a function defined in the index produces a `StoredAsPointer` edge.
- **QR-2.3** **[V1]** Files that fail to parse are reported; the user must be able to see the list of skipped files.
- **QR-2.4** **[V1]** The set of files indexed must equal `git ls-files` filtered by extension. No silent inclusion or exclusion.

### QR-3 Exportability *(stated user priority)*
- **QR-3.1** **[V1]** Mermaid and draw.io exports must be importable into the user's existing tooling without manual cleanup.
- **QR-3.2** **[V1]** All exports for the same query must represent the same underlying graph slice.
- **QR-3.3** **[V2]** SVG and PNG exports must match what the user sees in the in-app graph view.

### QR-4 Robustness
- **QR-4.1** **[V1]** Tree-sitter parse errors on individual files do not abort the index build.
- **QR-4.2** **[V1]** Subprocess failures (`git`) are reported with stderr content visible to the user.
- **QR-4.3** **[V2a]** `dot.exe` failures (timeout, non-zero exit) are reported with stderr; the rest of the UI remains usable.
- **QR-4.4** **[V1]** Memory usage stays under 4 GB for a 5 MLOC codebase. Above this, the tool warns and continues.

### QR-5 Determinism
- **QR-5.1** **[V1]** Given the same source-root and git commit, two indexing runs produce identical edge sets.
- **QR-5.2** **[V1]** Given the same `GraphSlice`, every export run produces byte-identical output (subject to header timestamp, which the user can disable).
- **QR-5.3** **[V2a]** Given the same `GraphSlice`, `dot.exe` produces an identical layout (`dot` is deterministic).
- **QR-5.4** **[V2b]** Given the same `GraphSlice` and the deterministic seed, the simulation is reproducible *up to floating-point rounding*. Steady-state layouts may differ in detail across machines. This is documented behaviour, not a bug.

## Constraints

### C-1 Platform
- **C-1.1** Windows only. Win64 MSVC build.
- **C-1.2** Standalone executable with statically linked Qt and tree-sitter.
- **C-1.3** **[V2a]** `dot.exe` ships beside the executable under `vendor/graphviz/`.

### C-2 Dependencies
- **C-2.1** Qt 6.8.3 exactly.
- **C-2.2** C++17, MSVC.
- **C-2.3** **[V1]** Only Qt + tree-sitter + tree-sitter-cpp.
- **C-2.4** **[V2a]** Adds Graphviz `dot.exe`. No source-level Graphviz dependency.
- **C-2.5** **[V2b]** Adds **no** new external dependency. Force model, integrator, and Barnes–Hut quadtree are written in-tree.

### C-3 Build
- **C-3.1** CMake ≥ 3.21.
- **C-3.2** No CI. Local builds via the per-platform PowerShell scripts.
- **C-3.3** Static linkage of Qt and tree-sitter into the final exe.

### C-4 Codebase being analysed
- **C-4.1** Pure C++98. No lambdas. No `auto`. No range-for. No `nullptr`. Heavy `#ifdef` / `#ifndef`.
- **C-4.2** Function pointers are common and must be a first-class edge kind.
- **C-4.3** Calls inside macro expansions are common and must be a first-class edge kind.
- **C-4.4** No Qt in the analysed codebase.
- **C-4.5** No compilation database. Build is MSVC + 100+ `.sln` files; not all compiled files appear in solutions.

## Out of scope

Explicitly **not** in scope across all versions:

- **OOS-1** Editing, refactoring, or modifying the analysed code in any way.
- **OOS-2** Cross-language analysis (C#, Java, Python, etc.).
- **OOS-3** Persistent indices, shared indices, server-mode operation.
- **OOS-4** Incremental re-indexing on file change.
- **OOS-5** Code review, lint, or static-analysis features beyond the call graph.
- **OOS-6** Mac, Linux, WebAssembly builds. Windows only.
- **OOS-7** Argument-type-based overload resolution.
- **OOS-8** Template instantiation tracking.
- **OOS-9** Reading `.sln` or `.vcxproj` files. Discovery is git-only.
- **OOS-10** Pretty themes, dark mode polish, branding.
- **OOS-11 [V2]** Implementing both `dot.exe` and physics layouts in the same release. Pick one.
- **OOS-12 [V2b]** "Publish-quality" output from the physics layout. Use the export formats for that.

## Acceptance criteria

### V1 acceptance

The V1 tool is "done enough to use" when:

1. Indexing a representative branch of the target codebase succeeds without crashing and reports any unparseable files.
2. Searching for a function name returns results in under 100 ms.
3. Selecting a result shows its callers and callees as lists in the details pane within 50 ms.
4. The user can navigate the graph by clicking caller/callee rows (back/forward).
5. Export of `callersOf(focus, depth=5) ∪ calleesOf(focus, depth=5)` produces valid Mermaid, DOT, draw.io, and JSON within 1 second.
6. Mermaid and draw.io exports of that slice open cleanly in the user's existing tooling.
7. All edge kinds (`DirectCall`, `MethodCall`, `StoredAsPointer`, `PassedToThread`, `ViaMacro`, `Unresolved`) are visually distinguishable both in the details panes (icons) and in the exports (line styles/colours).
8. Switching to a different git branch and re-indexing produces a different, branch-correct graph.

### V2a acceptance (incremental over V1)

V2a is done when:

1. The graph view renders any V1-valid slice within 2 seconds end-to-end (DOT generation + subprocess + parse + render) for slices up to 200 nodes.
2. Click-to-expand a boundary node re-renders the slice with depth+1 within 2 seconds.
3. SVG export of the visible graph view matches the on-screen rendering pixel-for-pixel (modulo antialiasing).
4. `dot.exe` failures (timeout, missing binary, non-zero exit) display an in-app error and leave the rest of the UI usable.

### V2b acceptance (incremental over V1)

V2b is done when:

1. The graph view sustains 60 fps for slices up to 300 nodes without the Barnes–Hut quadtree, and up to 5000 nodes with it.
2. Directional gravity produces visually-readable caller-above-callee layouts on representative slices from the user's codebase.
3. Dragging a node updates positions in real time without dropping below 30 fps during the drag.
4. The "freeze layout" toggle stops the simulation; the user can then export an SVG/PNG of the frozen state.
5. Re-opening the same slice (same root, depth, filters) produces a visually similar layout (deterministic seed, modulo floating-point variation).
