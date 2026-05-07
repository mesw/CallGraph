# ARCHITECTURE.md

Detailed component design for CallGraph. Read CLAUDE.md first for the orientation; this document is the "why and how" of the structure.

## Roadmap

The project ships in three phases. V1 is the current target. V2a and V2b are alternative add-ons; pick one after V1 ships.

### V1 — Indexer + search + exports (no graph view)

A CLI-feel GUI: a search box, a results list, a details pane showing callers and callees as lists, and an export button. The graph is virtual — it lives in the index and is materialised only by exports (Mermaid, DOT, draw.io, JSON, SVG, PNG). Mermaid is the headline format because it matches the user's existing workflow.

This is the smallest shippable version of the tool. It already replaces the `grep -rE` + manual classification + manual diagram drafting workflow. Most of the value is in the index and the exports, not in an interactive view.

**Components:** Discovery, Parser pool, Fact extractor, Resolver, Index, Query engine, Export, UI (search + details panes only).

### V2a — In-app graph view via Graphviz `dot.exe`

Adds an in-app graph panel that renders the current `GraphSlice` using `dot.exe -Tjson` for layout. Hierarchical (Sugiyama-style) layout — call graphs are directional, this matches caller→callee flow visually. Static positions, computed once per slice; click-to-expand on boundary nodes triggers a re-layout.

**New components:** `src/graph_dot/` (subprocess wrapper, JSON parser, position cache), `GraphView.qml`, `EdgeLayer` (custom `QQuickItem` rendering edges via `QSGGeometryNode`).

**New dependency:** `dot.exe` vendored under `vendor/graphviz/`.

### V2b — In-app graph view via in-process force-directed physics

Adds an in-app graph panel that lays out nodes via a continuous force-directed simulation: nodes repel each other, edges act as springs, optional directional gravity gives caller-above-callee bias. Runs on the QML render loop. The user can drag nodes; physics responds in real time.

**New components:** `src/graph_physics/` (force model, integrator, spatial index, layout driver), `GraphView.qml`, `EdgeLayer` (same custom `QQuickItem` interface as V2a — the renderer is layout-agnostic).

**No new external dependency.** Implemented in plain C++17.

### V2a vs V2b — which to pick

| Axis | V2a (dot) | V2b (physics) |
|---|---|---|
| Layout aesthetic | Hierarchical, deterministic, paper-like | Organic, cluster-revealing, animated |
| Direction of flow | Naturally top-to-bottom | Needs directional bias to look directional |
| Determinism | Same input → same layout | Depends on initial conditions and damping |
| Interaction | Pan / zoom / click-expand | Pan / zoom / click-expand / drag nodes |
| Performance | Bounded by `dot.exe`; ~1 s for hundreds of nodes; very slow for thousands | 60 fps for ~300 nodes naïve, ~5k nodes with Barnes-Hut |
| External binary | Yes (`dot.exe`) | No |
| Implementation effort | Lower (subprocess + JSON parse + spline render) | Higher (force model + integrator + spatial index) |
| Visual polish out of the box | High | Medium until tuned |

The user's existing workflow already produces hierarchical diagrams via Mermaid and draw.io. V2a doubles down on that aesthetic. V2b is a different way of looking at the same data — better for "what's the cluster structure around this function" than "what's the call chain from A to B."

## Top-level decisions and their rationale

| Decision | Choice | Why |
|---|---|---|
| Parser | tree-sitter C++ (not libclang) | Input is C++98 with heavy `#ifdef` and MSVC extensions; no compdb is available; include paths cannot be reliably synthesised. Tree-sitter parses unconfigured source robustly. |
| File discovery | `git ls-files` + extension filter | The codebase has 100+ `.sln` files referencing folders, and not all compiled files appear in solutions. Git is the only authoritative file set per branch. |
| Storage | In-memory only | Branch-scoped use, no need for cross-session persistence per the user's requirements. |
| Threading | Parallel parse, serial resolve, lock-free query | Index is built once then becomes immutable; the query engine runs without synchronisation. |
| GUI | Qt Quick / QML | Specified by the user. Qt 6.8.3. |
| V1 graph view | None | Most value sits in the index and the existing export workflow. Ship without graph view first. |

## Layer overview (V1)

```
┌─────────────────┐
│   Discovery     │  git ls-files → list of .h/.cpp paths
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  Parser pool    │  parallel; tree-sitter parse + fact extract
└────────┬────────┘
         │  Facts (FunctionDef, CallSite, FuncPtrAssign, MacroDef, ClassDecl...)
         ▼
┌─────────────────┐
│   Resolver      │  serial; name resolution, virtuals, macro expansion
└────────┬────────┘
         │  Edges with kind + confidence
         ▼
┌─────────────────┐
│     Index       │  immutable; symbol table + adjacency maps
└────────┬────────┘
         │
    ┌────┴────────┐
    ▼             ▼
┌─────────┐  ┌──────────┐
│  Query  │  │  Export  │
└────┬────┘  └────┬─────┘
     │            │
     ▼            ▼
┌─────────────────┐
│       UI        │  search pane, details pane (callers list, callees list)
└─────────────────┘
```

The flow is one-shot per session: launch → discover → parse → resolve → freeze index → user queries → user exports → exit.

## Discovery

**Inputs:** absolute path to source root (must be inside a git working tree), optional path-prefix filter.

**Outputs:** `std::vector<std::filesystem::path>` of files to parse.

**Algorithm:**
1. Run `git ls-files -z --full-name` in the source root via `QProcess`.
2. Filter by extension: `.h .hpp .hxx .inl .c .cpp .cxx .cc .ipp`.
3. Optionally apply user-supplied include/exclude path prefixes.
4. Drop generated files matching configurable patterns (default: `*/generated/*`, `*/build/*`).

**Why git, not `std::filesystem::recursive_directory_iterator`:** branches matter, and git skips ignored files (build outputs, third-party drops) for free.

**Failure mode:** if `git ls-files` fails (not a repo, git not on PATH), surface the error to UI; do not fall back to filesystem walk silently.

## Parser pool

**Inputs:** file list from Discovery.

**Outputs:** stream of fact records merged into the global fact buffer.

**Threading model:**
- One `QThreadPool` sized to `std::thread::hardware_concurrency()`.
- Each worker owns one `TSParser*` for the C++ grammar (tree-sitter parsers are not thread-safe; one-per-thread is correct).
- Each task: memory-map the file (`QFile::map`), parse to `TSTree`, run the fact-extraction visitor, push facts to a thread-safe MPSC queue, drop the tree before returning.

**Memory:** ASTs are dropped per-file; only extracted facts (flat POD records) survive. For a multi-MLOC codebase this keeps peak memory at a few hundred MB.

**Progress reporting:** atomic counters for `files_parsed` and `bytes_parsed`, polled from the UI thread on a 100 ms timer.

## Fact extractor

The fact extractor is a tree-sitter query-driven visitor. It does **not** resolve names — that is the resolver's job. It emits flat records:

```cpp
struct FunctionDef {
    SymbolId       id;
    std::string    qualified_name;     // e.g. "ns::Class::foo"
    int            arity;
    std::string    class_qname;        // empty if free function
    bool           is_virtual;
    bool           is_static;
    bool           is_definition;      // false for forward decls
    SourceLocation loc;
};

struct CallSite {
    SymbolId       enclosing_function; // who is making this call
    std::string    callee_name;        // unresolved identifier as written
    int            callee_arity;
    EdgeKind       kind;               // see §Edge taxonomy
    SourceLocation loc;
    bool           inside_macro_expansion;
};

struct FuncPtrAssign {
    SymbolId       enclosing_function;
    std::string    target_expression;  // e.g. "g_callbacks.on_event"
    std::string    source_function;    // e.g. "Foo"
    SourceLocation loc;
};

struct MacroDef {
    std::string    name;
    std::string    body;
    std::vector<std::string> body_calls; // function-like call tokens inside body
    SourceLocation loc;
};

struct ClassDecl {
    std::string    qualified_name;
    std::vector<std::string> base_classes;
    SourceLocation loc;
};

struct MethodDecl {
    std::string    class_qname;
    std::string    method_name;
    int            arity;
    bool           is_virtual;
    bool           is_override;        // best-effort; C++98 has no `override`
    SourceLocation loc;
};
```

**Tree-sitter queries** (one `.scm` file per fact type, loaded at startup) live in `src/parser/queries/`:

- `function_def.scm` — captures `function_definition` nodes plus their qualified declarator.
- `call_site.scm` — captures `call_expression` and `field_expression`-rooted calls.
- `func_ptr_assign.scm` — captures `assignment_expression` where RHS is `&identifier` or bare identifier matching a known function name.
- `macro_def.scm` — captures `preproc_function_def` and `preproc_def`.
- `class_decl.scm` — captures `class_specifier` and `struct_specifier` with base lists.

**Edge kind during extraction** is a first guess based on syntactic context:

- Direct identifier call → `DirectCall`
- `obj.f()` or `ptr->f()` → `MethodCall`
- Inside a `preproc_arg` or expansion of a known macro → set `inside_macro_expansion=true`
- Argument to a known thread-spawn API (configurable list: `CreateThread`, `_beginthreadex`, `pthread_create`, ...) → `PassedToThread`
- Assigned to a function pointer → `StoredAsPointer` (emitted as `FuncPtrAssign`, not `CallSite`)

**Macros that paste tokens** (`Do##x`) are recognised and the call is emitted with kind `Unresolved` and the macro body text attached. No attempt at expansion.

## Resolver

Single-threaded, runs after all parsing completes. Operates on the merged fact buffer and produces the final `Index`.

**Step 1 — Symbol table.**
Build `unordered_map<QName, vector<SymbolId>>` from all `FunctionDef` records where `is_definition == true`. Multiple entries per name represent overloads.

**Step 2 — Class hierarchy.**
Build a `unordered_map<QName, vector<QName>>` from `ClassDecl` records: subclass → bases. Compute the transitive closure once.

**Step 3 — Resolve calls.**
For each `CallSite`:
1. Look up `callee_name` (after qualifying with the enclosing scope's namespaces) in the symbol table.
2. Filter candidates to those with matching arity.
3. Bucket the result:
   - **0 matches** → emit `Edge{ kind=Unresolved, target=synthetic_node(callee_name) }`.
   - **1 match** → emit edge with `confidence=Exact`.
   - **>1 matches** → emit one edge per candidate with `confidence=Overload`.

**Step 4 — Function pointer edges.**
For each `FuncPtrAssign`, look up `source_function` in the symbol table. Emit `Edge{ kind=StoredAsPointer }`. If the assignment target syntactically matches a known thread-API parameter, override kind to `PassedToThread`.

**Step 5 — Virtual override expansion** *(feature-flagged, off by default)*.
For each call resolved to a method `M` declared `virtual`:
1. Find all subclasses of `M`'s declaring class via the hierarchy map.
2. For each subclass, look up methods with the same name+arity.
3. Emit additional edges with `kind=VirtualCandidate, confidence=Heuristic`.

**Step 6 — Macro edges.**
For each `MacroDef`, parse `body_calls` as identifiers and emit `Edge{ source=macro_node, target=resolved_function, kind=ViaMacro }`. The macro itself is a node in the graph.

## Index

```cpp
class Index {
    std::unordered_map<SymbolId, FunctionDef> m_symbols;
    std::unordered_map<std::string, std::vector<SymbolId>> m_byName;
    std::vector<Edge> m_edges;
    std::unordered_map<SymbolId, std::vector<EdgeId>> m_forward;  // caller → edges
    std::unordered_map<SymbolId, std::vector<EdgeId>> m_reverse;  // callee → edges
    std::vector<std::string> m_allNames; // sorted, deduplicated, for substring search
};
```

**Immutability:** after `Resolver::finalize()` returns, the `Index` is `const` for the rest of the session. The query layer takes a `const Index&` and runs without locking.

**Memory budget target:** under 1 GB resident for a 5 MLOC codebase with ~10M edges. If exceeded, first cuts are string interning for qualified names and packed `Edge` records.

## Edge taxonomy

| `EdgeKind`         | Meaning                                      | Confidence  |
|--------------------|----------------------------------------------|-------------|
| `DirectCall`       | `f();` or `ns::f();`                         | Exact / Overload |
| `MethodCall`       | `obj.f();` or `ptr->f();`                    | Exact / Overload |
| `VirtualCandidate` | possible override target of a virtual call   | Heuristic   |
| `StoredAsPointer`  | `fp = &f;` or `register_cb(&f);`             | Exact       |
| `PassedToThread`   | function passed to a known thread-spawn API  | Exact       |
| `ViaMacro`         | call appears inside a macro expansion        | Exact / Overload |
| `Unresolved`       | callee not found in index (likely external)  | n/a         |

```cpp
enum class Confidence { Exact, Overload, Heuristic, Unknown };

struct Edge {
    SymbolId   from;
    SymbolId   to;
    EdgeKind   kind;
    Confidence confidence;
    SourceLocation loc;
    std::optional<std::string> via_macro;
};
```

## Query engine

```cpp
GraphSlice callersOf(const Index&, SymbolId root,
                     int max_depth,
                     EdgeKindMask filter,
                     ConfidenceMask confidence);

GraphSlice calleesOf(const Index&, SymbolId root,
                     int max_depth,
                     EdgeKindMask filter,
                     ConfidenceMask confidence);
```

Reverse / forward BFS over the adjacency maps. `GraphSlice` is the input format for the details pane (V1) and the graph view (V2):

```cpp
struct GraphSlice {
    SymbolId                root;
    std::vector<NodeRef>    nodes;
    std::vector<EdgeRef>    edges;
    QueryParams             params;
};
```

**Name search:**

```cpp
std::vector<SymbolId> findByName(const Index&, std::string_view query, SearchMode mode);
// modes: Substring, Regex, ExactQualified
```

## UI

### V1 UI

No graph rendering. The window splits into three panes:

- **Search pane (left):** input box, mode toggle (substring / regex / exact-qualified), results list driven by `SearchModel` (`QAbstractListModel`). Each result row shows qualified name, signature, file, line.
- **Focus pane (centre):** information about the currently focused symbol — qualified name, full signature, source location with a "open in editor" button (uses `QDesktopServices::openUrl` with a configurable `vscode://`, `idea://`, or `notepad++` URL scheme).
- **Details pane (right, two stacked lists):**
  - **Callers** — `EdgeListModel`, every edge in `m_reverse[focus]`. Columns: caller name, edge kind icon, confidence icon, file:line.
  - **Callees** — `EdgeListModel`, every edge in `m_forward[focus]`. Same columns.
  - Click on a row → focus that symbol. Implements a "navigate the graph by clicking lists" UX without rendering a graph.

A toolbar above the panes hosts: source-root picker, "rebuild index" button, depth selector (used only by the export — V1 has no graph view), filter toggles (edge kinds, confidence levels), "export" button.

The export dialog is the same as in later versions: format dropdown, depth selector, file picker. The slice exported is `callersOf(focus, depth) ∪ calleesOf(focus, depth)`.

### V2a UI additions — graph view via Graphviz

A fourth pane (or a tab in the centre) holds `GraphView.qml`. Backing it:

- **`DotLayout`** (in `src/graph_dot/`) — takes a `GraphSlice`, emits DOT, runs `vendor/graphviz/dot.exe -Tjson` via `QProcess`, parses the JSON for node positions and edge spline control points, returns a `LayoutedGraph`:

  ```cpp
  struct LayoutedNode { SymbolId id; QPointF pos; QSizeF size; };
  struct LayoutedEdge { EdgeId id; std::vector<QPointF> spline; };
  struct LayoutedGraph { std::vector<LayoutedNode> nodes;
                        std::vector<LayoutedEdge> edges; };
  ```

- **`GraphModel`** — exposes `LayoutedGraph` to QML.
- **`EdgeLayer`** (custom `QQuickItem`) — single scene-graph node drawing all edges as antialiased line strips via `QSGGeometryNode`. Per-kind colour and dash pattern.
- **Node rendering** — `Repeater` of `Rectangle` items positioned absolutely from `LayoutedNode::pos`.
- **Interaction** — pan via drag on background, zoom via wheel, click on a node to focus, click on a boundary node to expand depth by 1.

**Layout cache:** `unordered_map<GraphSliceHash, LayoutedGraph>`. Slices are deterministic given (root, depth, filter), so a cache hit is free. Eviction: LRU with cap 32.

**Subprocess robustness:** `dot.exe` runs with a 10 s timeout. On timeout or non-zero exit, render the fallback "layout failed — try a smaller depth" overlay; do not crash. Stderr is shown in a collapsible panel.

### V2b UI additions — graph view via force-directed physics

Same QML structure as V2a (`GraphView.qml`, `EdgeLayer` rendering). The renderer is layout-agnostic: it consumes a `LayoutedGraph` from whichever layout source. The difference is the layout engine.

See **§V2b graph view — physics engine** below for the simulation details.

## V2a graph view — Graphviz integration

**DOT generation rules:**
- One node per `SymbolId` in the slice; label is the unqualified name (qualified name in tooltip).
- Node `style` attribute encodes class membership (rounded for free functions, box for methods).
- Edge `style` attribute encodes `EdgeKind` (per the colour/style table in §Export).
- Graph attributes: `rankdir=TB`, `splines=true`, `overlap=false`, `nodesep=0.3`, `ranksep=0.5`.

**Invocation:**
```
dot.exe -Tjson -Kdot
```

`-Tjson` returns positions and bounds in points (72 dpi). Convert to QML pixels at 1 point = 1.33 px (or whatever the user's DPI is).

**Spline parsing:** the JSON's `_draw_` array contains `B`-prefixed entries with cubic Bezier control points. Convert each into a `QPainterPath` or, since we render via scene graph, a polyline approximation at fixed t-step (default 16 segments per Bezier).

**Click-to-expand:** when a node at the slice boundary is clicked, recompute the slice with depth+1 in that direction, re-layout, animate node positions to new layout via `Behavior on x/y` over 300 ms.

## V2b graph view — physics engine

The simulation is the meat of V2b. This section is detailed because the design choices matter for both correctness and performance.

### Force model

Fruchterman–Reingold variant with directional bias.

For ideal edge length `k`, with `area` derived from the viewport size and `n` the node count:
```
k = C * sqrt(area / n)             // C ≈ 1.0; tunable
```

Per node `v`, sum the forces:

**Repulsive** (every pair `(u, v)`):
```
F_rep(u, v) = -(k² / |d|) * d_unit
```
where `d = pos[v] - pos[u]`. This is the F-R formula. Soft floor `|d| ≥ 1.0` to prevent singularity.

**Attractive** (along each edge `(u, v)`):
```
F_attr(u, v) = (|d|² / k) * d_unit
```

**Directional gravity** (since call graphs are directed):
```
F_dir(v) = (0, gravity_strength * depth(v))
```
where `depth(v)` is the BFS depth from the slice root computed once when the slice is constructed (callers get negative depth, callees positive). This pulls callers up and callees down without forcing strict hierarchy. `gravity_strength` ≈ k * 0.5; tunable.

**Centring** (weak pull toward viewport centre to prevent drift):
```
F_centre(v) = -centre_strength * (pos[v] - centre)
```

### Integration

Velocity Verlet with damping:
```
acc[v]      = sum_of_forces(v) / mass[v]      // mass = 1.0 default
vel[v]     += acc[v] * dt
vel[v]     *= damping                          // damping ≈ 0.85
pos[v]     += vel[v] * dt
```

Fixed `dt = 1/60`. Damping prevents oscillation; without it the system rings. A "temperature" cooling schedule (multiplying max displacement by a decreasing factor) is **not** used because it makes the layout feel dead after a few seconds; we want continuous responsiveness.

### Spatial index for repulsion

Naïve all-pairs repulsion is O(N²). Acceptable up to ~300 nodes at 60 fps. Above that, use a Barnes–Hut quadtree:

- Build a quadtree over node positions each frame (rebuild is cheaper than incremental update for our scales).
- Approximate clusters of nodes whose bounding-box `s/d < theta` as a single point at their centre of mass. `theta ≈ 0.5`.
- Drops repulsion to O(N log N). Sufficient for ~5k nodes at 60 fps on a current dev box.

For our use case (slices of 50–500 nodes most of the time), the quadtree is overkill but cheap to add and saves us from a rewrite if a user opens a thousand-node neighbourhood.

### Lifecycle

- Driven by `QQuickWindow::frameSwapped` — one physics step per rendered frame.
- The simulation never "ends" in the V1 sense, but it does become quiescent. A dirty flag (`m_settled`) is set when the maximum nodal kinetic energy across all nodes falls below a threshold for 60 consecutive frames. While settled, frames still render but the integrator skips work.
- Any of the following un-settles the simulation: user drag, user click-to-expand, slice change, viewport resize.

### Pinning and dragging

- Each node has a `pinned` flag. While `pinned`, forces are computed but `pos[v]` is **not** updated. This lets the simulation react to a pinned node without moving it.
- During a pointer drag, the dragged node is implicitly pinned and `pos[v]` is set from cursor coordinates each frame.
- Optional UX: long-press to toggle a permanent pin.

### Picking

- Node hit-testing via the same quadtree. O(log N) per pick.
- Edge hit-testing via point-to-line-segment distance with a 6 px slop. Linear in edges; acceptable for our scale.

### Initial conditions

Random placement in a circle of radius `k * sqrt(n)` with deterministic seeding from the slice root's `SymbolId`. Deterministic seeding means re-opening the same slice produces the same starting configuration (and, after enough damping, the same steady state — within rounding).

### Edge rendering with physics

Edges in V2b are straight lines (or short Catmull–Rom curves with 1 mid-point if visual softness is desired). Splines are not appropriate for a continuously-moving layout — the Bezier control points would have to be recomputed every frame and the curves would jitter visibly.

Same `EdgeLayer` `QQuickItem` as V2a; just feed it different geometry.

### Tuning constants summary

| Constant | Default | Range | Notes |
|---|---|---|---|
| `C` (k scale) | 1.0 | 0.5 – 2.0 | Larger → looser layout |
| `damping` | 0.85 | 0.7 – 0.95 | Lower → faster settle, more jitter |
| `gravity_strength` | 0.5 * k | 0 – 2 * k | 0 disables directional bias |
| `centre_strength` | 0.01 | 0 – 0.1 | Prevents drift |
| `theta` (Barnes–Hut) | 0.5 | 0.3 – 0.8 | Higher → faster, less accurate |
| `dt` | 1/60 | fixed | Tied to frame rate |

Expose these as developer-only sliders behind a hidden hotkey (Ctrl+Shift+P) — useful while tuning, hidden in production use.

## Export

All exporters take a `const GraphSlice&` and a `std::ostream&`. Adding a new format is a single file in `src/export/`.

| Format | Class | Notes |
|---|---|---|
| Mermaid | `MermaidExporter` | `flowchart TD`, edge style varies by `EdgeKind`. Matches the user's current workflow. |
| Graphviz DOT | `DotExporter` | Same content the V2a layout pass uses; ships even when used standalone. |
| draw.io XML | `DrawioExporter` | mxGraph format, importable into draw.io / diagrams.net. |
| JSON | `JsonExporter` | Stable schema documented in `src/export/json_schema.md`. |
| SVG (V1) | `SvgTextExporter` | Hand-rolled emit using DOT layout in-process *or* a pre-laid mermaid render. In V1 with no in-app layout, this is the only path that needs care; defer if not needed. |
| SVG (V2a/V2b) | `SvgSceneExporter` | Re-renders the laid-out graph view to SVG using the same node/edge geometry as the on-screen view. |
| PNG | `PngExporter` | `QQuickItemGrabResult::saveToFile` of the graph view (V2). In V1, render via offscreen Qt drawing of the slice with a simple list-style layout, or omit. |

**V1 reality check on SVG/PNG:** without an in-app graph view, V1 has no laid-out scene to grab. Two pragmatic options:

1. **Defer SVG/PNG to V2** — V1 ships Mermaid, DOT, draw.io, JSON only. Users who want raster/vector output run Mermaid or DOT through their existing tooling.
2. **In V1, generate SVG by shelling out to `dot.exe`** if the user has Graphviz installed (not vendored, just optional). Skip PNG.

Option 1 is cleaner. Option 2 is one PR if a user complains.

**Edge styling conventions** (consistent across all formats):

| EdgeKind         | Line style       | Colour    |
|------------------|------------------|-----------|
| DirectCall       | solid            | black     |
| MethodCall       | solid            | black     |
| VirtualCandidate | dashed           | blue      |
| StoredAsPointer  | dotted           | dark green|
| PassedToThread   | dotted, thick    | red       |
| ViaMacro         | solid, label "↻"| grey      |
| Unresolved       | solid, faded    | light grey|

## Threading model summary

| Phase     | Threads | Mutability of Index |
|-----------|---------|---------------------|
| Discovery | 1 (UI dispatches `QProcess`) | n/a |
| Parsing   | N (QThreadPool) | facts being appended to MPSC queue |
| Resolving | 1 | Index being mutated |
| Querying  | 1 (UI thread) | **Immutable** — no locks |
| Exporting | 1 | Immutable |
| V2a layout | 1 (UI thread; `dot.exe` runs in its own process) | Immutable |
| V2b physics | 1 (UI thread, frame-synced) | Immutable index; mutates per-node `pos` and `vel` arrays which are layout state, not index state |

## Dependencies

| Library | Version | Linkage | Required for |
|---|---|---|---|
| Qt | 6.8.3 | static | V1 |
| tree-sitter | latest stable | static, vendored | V1 |
| tree-sitter-cpp | latest stable | static, vendored | V1 |
| Graphviz `dot.exe` | 9.x | external binary, vendored under `vendor/` | V2a only |

V2b adds **no new external dependency**. The Barnes–Hut quadtree, force model, and integrator are all written in plain C++17 in `src/graph_physics/`.

Adding any other dependency requires updating this section and CLAUDE.md.

## Known limitations

1. **Overload disambiguation by argument types is not performed.** Calls resolve on `name + arity`. Multiple matches → all candidates listed.
2. **Virtual dispatch is over-approximated** when the override-expansion flag is on.
3. **Token-pasting macros are not expanded.** Calls inside `Do##x()` are marked `Unresolved`.
4. **Generated code outside git is invisible.**
5. **No incremental rebuild.**
6. **(V1 only)** No in-app visual graph; users see graphs via export.
7. **(V2a only)** Layout latency is bounded by `dot.exe`; very large slices may take seconds.
8. **(V2b only)** Layout is non-deterministic in detail (deterministic in seed, but small floating-point variations across runs); not suitable as a "publish quality" diagram source. For publishable diagrams, use exports (Mermaid/DOT/draw.io).

## Risks and mitigations

| Risk | Likelihood | Mitigation |
|---|---|---|
| Tree-sitter mis-parses heavily-conditional code | Medium | tree-sitter has `ERROR` recovery; we still extract facts from the parts that parsed. Log a warning per file with errors. |
| Memory usage explodes on very large repos | Medium | String interning fallback (see §Index). Profile early. |
| (V2a) Graphviz subprocess unreliable on Windows | Low | Vendor `dot.exe` directly in the install directory; do not rely on PATH. |
| (V2a) `dot.exe` slow on large slices | Medium | Cap default depth at 2; cache layouts per slice hash. |
| (V2b) Physics looks like spaghetti for high-fanout nodes | Medium | Tune `gravity_strength` and `damping`. Provide a "freeze layout" toggle so users can stop the simulation when it looks good. |
| (V2b) Performance cliff above N nodes | Medium | Implement Barnes–Hut from day one rather than as an optimisation. Linear-scan force model is acceptable as a fallback below 200 nodes. |
| Function-pointer dispatch tables underdetected | Medium | Add a dedicated tree-sitter query for initializer lists assigning `&function` to designated fields. |
| MSVC-specific syntax (`__declspec`, `__forceinline`, `__interface`) confuses tree-sitter | Low–Medium | tree-sitter-cpp tolerates most of these as attributes. Catalogue any failures and add grammar patches if needed. |
