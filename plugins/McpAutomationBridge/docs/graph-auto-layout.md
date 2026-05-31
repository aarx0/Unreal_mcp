# Blueprint Graph Auto-Layout — Design & Task

Spec for laying out Blueprint graph nodes the MCP bridge creates (and, optionally,
arbitrary graphs) so they don't pile up at the same coordinates. Self-contained:
a fresh implementer should need only this file + the bridge source.

## Background

The bridge (native UE 5.7 C++ plugin, `McpAutomationBridge`) authors Blueprint graph
nodes programmatically — e.g. `bind_on_clicked`, `bind_on_value_changed` in
`Source/McpAutomationBridge/Private/McpAutomationBridge_WidgetAuthoringHandlers.cpp`.
Today those handlers place every node at **hardcoded coordinates** (event at `(0,0)`,
conv at `(280,…)`, setText at `(560,0)`, …). So when a graph gets several generated
chains they all land on the same pixels and overlap ("smooshed"). We want clean layout.

This is a **throwaway bridge** (delete when Epic ships official UE MCP) — favor small,
deletable, dependency-free solutions over a real layout engine.

## Two separate problems — do them in this order

### Phase 1 — Generator self-layout (cheap, do first, high ROI)
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
  `propertyName` `X`/`NodePosX` and `Y`/`NodePosY` (see
  `McpAutomationBridge_BlueprintGraphHandlers.cpp` ~line 1470). Nodes report `x`/`y` in
  `get_graph_details` / `get_node_details`. So no new primitive is needed to move nodes.
- **Generators to fix (Phase 1)**: `bind_on_clicked`, `bind_on_value_changed` in
  `McpAutomationBridge_WidgetAuthoringHandlers.cpp` (they set `NodePosX/NodePosY` directly
  on the `UK2Node*`s right after `FGraphNodeCreator::Finalize`).
- **Adding `arrange_graph` (Phase 2)**: implement a handler that loads the graph
  (`UEdGraph` from `WidgetBP->UbergraphPages[...]` or a named graph), classifies each
  `UEdGraphNode` (exec pins via `UEdGraphSchema_K2::PC_Exec`; pure = no exec pins),
  computes the layout above, writes `NodePosX/NodePosY`, then
  `FBlueprintEditorUtils::MarkBlueprintAsModified`. Route it: add the action string to
  `WidgetAuthoring()` (or a graph group) in `MCP/McpConsolidatedActionRouting.h` **and**
  the tool's action enum in `MCP/Tools/McpTool_ManageBlueprint.cpp`, then rebuild
  (header change ⇒ full rebuild; see `docs/extending-the-bridge.md`).
- Node sizes vary; approximate with a fixed column `GAP` (~320px) and row height from pin
  count, or call the node's estimated size. Pixel-perfect isn't required.

## Suggested scope

1. **Phase 1** — generator self-layout for the two existing binders. Ships with the next
   bridge commit. ~20 lines total. (This alone fixes the reported problem.)
2. **Phase 2** — `arrange_graph` with exec-X + cursor-Y, linear + branch support, no
   crossing-minimization. Build only on demand.

Branch convention: land bridge changes on the fork's default branch `dev`. Build/Live-
Coding workflow in `docs/extending-the-bridge.md`.
