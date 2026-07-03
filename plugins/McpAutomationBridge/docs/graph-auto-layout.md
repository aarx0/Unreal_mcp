# Blueprint Graph Auto-Layout — Design & Task

Spec for laying out Blueprint graph nodes the MCP bridge creates (and, optionally,
arbitrary graphs) so they don't pile up at the same coordinates. Self-contained:
a fresh implementer should need only this file + the bridge source.

## Background

The bridge (native UE 5.7 C++ plugin, `McpAutomationBridge`) authors Blueprint graph
nodes programmatically — e.g. `bind_on_clicked`, `bind_on_value_changed` in
`Source/McpAutomationBridge/Private/McpAutomationBridge_WidgetAuthoringHandlers.cpp`.
Each chain uses hardcoded **relative** offsets (event at `(0,0)`, conv at `(280,…)`,
setText at `(560,0)`, …). Originally every chain also *started* at the same origin, so
multiple generated chains landed on the same pixels and overlapped ("smooshed") — fixed
by Phase 1 below, which is now shipped.

This is a **throwaway bridge** (delete when Epic ships official UE MCP) — favor small,
deletable, dependency-free solutions over a real layout engine.

## Two separate problems — do them in this order

### Phase 1 — Generator self-layout (SHIPPED)
The handlers *know the exact shape they create*, so they don't need a layout algorithm —
just emit **non-overlapping relative coordinates**. Before placing a new event chain,
offset the whole chain vertically so it sits below previously-generated chains.

- Count existing root/event nodes (or track a static Y baseline) and start the new
  chain at `baseY = existingEventChains * ROW_GAP` (ROW_GAP ≈ 300).
- Within the chain keep the relative offsets they already use (event at x=0,
  pure feeders to the left of their consumer, exec successor to the right).
- Fixes the smoosh permanently for `bind_on_clicked` / `bind_on_value_changed` with
  ~10 lines each. **No graph traversal needed.**

### Phase 2 — Generic `arrange_graph` action (optional, only if needed)
Lay out an *arbitrary* graph we didn't generate (or that a human edited). This is the
real algorithm below. **Only build this if we end up needing headless arrange of
arbitrary graphs** — otherwise a human can press the BlueprintAssist hotkey (see Tools).

## The algorithm (for Phase 2)

It's **Sugiyama / layered layout, specialized for Blueprints**: rank by *execution*
flow, not by treating all edges equally.

**X (columns) — drive off exec (white) wires.**
- Roots = event nodes (no exec input). Walk exec **forward**.
- `node.X = max(exec-predecessor.X) + nodeWidth + GAP`. A node with multiple exec inputs
  (a merge) goes right of the **rightmost** predecessor → place in **topological order**.
- Break exec cycles first (reverse back-edges to get a DAG), or longest-path won't terminate.

**Pure / data nodes (no exec pins) — anchor to their consumer, not their inputs.**
- A pure node (getter, `To Text`, math) has no exec pin, so the exec walk never reaches it.
  Do a second pass: walk **backward** from each placed node through its **data-input** pins.
- Place a pure feeder **left** of its consumer: `feeder.X = consumer.X - nodeWidth - GAP`.
- Do NOT longest-path pure nodes from their own inputs — a lone getter has no inputs and
  would wrongly land in column 0 next to the events. Anchor to the consumer.

**Y (rows) — single non-resetting cursor + parent-centering** (Reingold–Tilford /
Sugiyama coordinate phase):
- DFS the combined exec+data tree. Each **leaf** takes the next row off a monotonic Y
  cursor; each **parent centers** on its children's rows.
- Stacked feeders of one node (e.g. two data inputs) land on adjacent rows → no overlap.
- **Branches** (multiple exec-*outs*: Branch, Sequence) → each branch is its own subtree
  on its own rows.
- Independent event trees stack for free: the cursor is never reset, so the next event
  starts below the last tree — and it's right-sized (uses the previous tree's real height,
  not a constant guess).

## Worked example / test case

Our `bind_on_value_changed` output (the graph that smooshed) is the minimal test:

```
Event OnValueChanged(VolumeSlider) ──exec──▶ SetText(VolumeValue)
                                               ▲    ▲
              Conv ToText(Float) ────data──────┘    │   (feeds InText)
              Get VolumeValue ───────data───────────┘   (feeds Target/self)
              Event.Value ─────data──▶ ToText.Value
```

Correct layout (one chain):
```
ToText           row 0   col 1
Get VolumeValue  row 1   col 1
SetText          row 0.5 col 2     (centers on its two feeders)
Event            row 0.5 col 0     (exec root; aligns to SetText's row)
```
A second binding (DeadZone) must start at row 2 (cursor advanced), not on top of this.
Acceptance: no two nodes share a bounding box; exec reads left→right; feeders sit left
of `SetText` on distinct rows.

## Tools / prior art (why we don't embed a layout kernel)

- **Sugiyama (layered)** is the standard algorithm; everything below implements a variant.
- **ELK** — best-in-class, uniquely good because it does **port-constraint** layout (lays
  out around pins, exactly the Blueprint problem). But it's **Java** → can't embed in C++.
- **dagre** — compact Sugiyama (what most web node-editors use). Good reference, but JS.
- **Graphviz `dot`** — C, classic layered layout, but weak pin/port model.
- **OGDF (C++) / MSAGL (C#)** — high quality, embeddable in-language, but heavyweight deps.
- **BlueprintAssist** (UE Marketplace plugin) — already does Blueprint-aware (exec/pin)
  auto-formatting on a hotkey. **The "does it well" answer for a human in-editor.**

Recommendation: don't embed any kernel. Self-layout what we generate (Phase 1); point
humans at BlueprintAssist; only if we truly need programmatic/headless arrange of
arbitrary graphs, port a small dagre-style pass (~150–200 lines), skip crossing-
minimization, accept "fine."

## Bridge integration points

- **Setting node position already works**: `manage_blueprint set_node_property` accepts
  `propertyName` `X`/`NodePosX` and `Y`/`NodePosY` (see the `set_node_property` branch in
  `McpAutomationBridge_BlueprintGraphHandlers.cpp`). Nodes report `x`/`y` in
  `get_graph_details` / `get_node_details`. So no new primitive is needed to move nodes.
- **Generators (Phase 1 — SHIPPED)**: `bind_on_clicked`, `bind_on_value_changed` in
  `McpAutomationBridge_WidgetAuthoringHandlers.cpp` offset each new chain by
  `BaseY = ExistingEventChains * McpWidgetChainRowGapY` (300px) before setting
  `NodePosX/NodePosY`, so generated chains stack instead of overlapping.
- **`arrange_graph` (Phase 2) — IMPLEMENTED** in `HandleBlueprintGraphAction`
  (`McpAutomationBridge_BlueprintGraphHandlers.cpp`): `ArrangeBlueprintGraph` classifies each
  `UEdGraphNode` (exec pins via `UEdGraphSchema_K2::PC_Exec`; pure = no exec pins) and
  translates the graph into `McpGraphLayout::FLayoutNode/FLayoutEdge`; the column/row
  placement algorithm lives in the shared, UE-agnostic `McpGraphLayoutUtils.cpp` (reused by
  the Material-graph `arrange_graph` in `McpAutomationBridge_MaterialAuthoringHandlers.cpp`).
  The adapter writes the returned `NodePosX/NodePosY` back, then `MarkBlueprintAsModified` +
  save. **Routing is one list, not two:** add the action to `BlueprintGraph()` in
  `MCP/McpConsolidatedActionRouting.h`. That single array both gates dispatch (the
  `manage_blueprint` lambda routes via `McpConsolidatedActions::IsBlueprintGraphAction`;
  anything not listed falls through to `HandleBlueprintAction` and returns `UNKNOWN_ACTION`)
  and supplies the schema (`McpTool_ManageBlueprint.cpp` builds the client `action` enum from
  `McpConsolidatedActions::ManageBlueprint()`, which unions it in — no hand-edited enum).
  `get_nodes` has a handler but is absent from `BlueprintGraph()`, so it's still unreachable.
  Header touch ⇒ full rebuild (editors closed). See `docs/extending-the-bridge.md`.
- Node sizes vary; approximate with a fixed column `GAP` (~320px) and row height from pin
  count, or call the node's estimated size. Pixel-perfect isn't required.

## Status

1. **Phase 1** — SHIPPED: generator self-layout for the two existing binders.
2. **Phase 2** — SHIPPED: `arrange_graph` (Blueprint + Material graphs, shared
   `McpGraphLayoutUtils` core).

Branch convention: land bridge changes on the fork's default branch `main`. Build/Live-
Coding workflow in `docs/extending-the-bridge.md`.
