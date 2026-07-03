# Material-authoring regression test

A self-contained integration test for the `manage_asset` material value-setters and compile
read-backs, run over the MCP bridge — no engine rebuild (it composes existing actions). Unlike
the declarative `tests/ui-nav` specs (which need a bespoke runner), this is a single assertive
script: editor + bridge must be up.

Run:
```
pwsh -File tests/material/material-authoring-test.ps1
```
Exits non-zero if any assertion fails; self-cleans the scratch material even on failure.

## Coverage (18 assertions, green 2026-06-28)
- `set_node_value` on `Constant3Vector` (r/g/b), `ScalarParameter` (value→DefaultValue), and an
  arithmetic node (`constA`/`constB`, the reflection path), each confirmed by a `get_node_properties`
  read-back of the written value.
- `set_node_value` `UNSUPPORTED_NODE` error path on a non-settable node (TextureCoordinate).
- `get_material_info` reports `compiled` (and omits `errors[]` on a valid material).
- `get_material_stats` reports `compiled` + an honest `instructionCountNote`.
- `compile_material` reports `compiled:true` on a valid material.
- `compile_material {waitForShaders}` catches a real shader-**backend** error (guaranteed-invalid HLSL
  in a Custom node): the fast path is blind (`compiled:true`) while `waitForShaders` returns
  `compiled:false` + `errors[]` with the DXC message.

Guards the features in commits `a612c523` (set_node_value), `ca2c4564` (read-back errors[]), and the
validated `waitForShaders` shader-backend-error path.
