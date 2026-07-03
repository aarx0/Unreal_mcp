# Safe Asset Saving in the Native Bridge

How to persist a `UObject` asset from a bridge handler without crashing the editor.
Self-contained: read this + `Private/McpSafeOperations.h` and you have the full picture.

## TL;DR

| Saving… | Use | Why |
|---------|-----|-----|
| A newly created / duplicated asset, or anything where editor-side checkout + asset-registry rescan are wanted | `McpSafeOperations::McpSafeAssetSave` | Routes through the editor's save helpers (registry rescan, source-control checkout). |
| An **edited existing** asset in an **unattended** context (e.g. `set_asset_property`) | `McpSafeOperations::McpDirectPackageSave` | Raw `UPackage::Save` straight to disk — bypasses the validate-on-save and source-control hooks that can crash the editor. |

Both live in `McpSafeOperations.h`. **Never** call raw `UPackage::SavePackage` / `UEditorAssetLibrary::SaveLoadedAsset` directly from a handler.

## Modal-freeze guard

`McpSafeAssetSave` and `McpSafeLevelSave` wrap their editor-save calls
(`SavePackagesForObjects` / `PromptForCheckoutAndSave` / `FEditorFileUtils::SaveLevel`) in
`GIsRunningUnattendedScript = true`, restored on every exit path (see `McpSafeOperations.h`).
Without the guard, a save failure pops the editor's Cancel/Retry/Continue modal, whose nested
Slate loop runs on the bridge's game thread and starves every subsequent MCP call until a human
clicks a button. With it, `FMessageDialog::Open` returns its default answer and a real failure
surfaces as `false` plus a logged warning.

## Background — two real crashes

The editor's "nice" save paths run extra pipelines *around* the actual disk write, and
on some projects those pipelines fault. We hit **two distinct access violations** saving
the *same* asset (`/Game/Audio/Control_Mixes/CB_Volume`) via `set_asset_property`:

1. **Validate-on-save → `UnrealEditor-DataValidation`.**
   `UEditorAssetLibrary::SaveLoadedAsset(Asset, false)` runs the editor's content
   validators as part of the save. A validator faulted while validating the asset:
   ```
   …DataValidation.dll  (×6 frames)
     → UnrealEd → Core  → EXCEPTION_ACCESS_VIOLATION
   ```

2. **Checkout-on-save → `UnrealEditor-GitSourceControl`.**
   Switching to `McpSafeAssetSave` removed the validation crash, but that helper still
   routes through `UPackageTools::SavePackagesForObjects` /
   `FEditorFileUtils::PromptForCheckoutAndSave`, which invoke source control. The Git
   provider then faulted doing a Slate operation during the save flow:
   ```
   McpSafeAssetSave (McpSafeOperations.h)
     → UnrealEd → SourceControl → GitSourceControl → Slate → Core  → AV
   ```

The common denominator: **any editor-integrated save pipeline is a liability for
unattended automation.** Validation and source-control are editor *conveniences*, not
requirements for writing a `.uasset`. So for programmatic edits we skip them and write
the package directly.

> Note: both crashes are *latent project issues* independent of the bridge — a manual
> `Ctrl+S` of the offending asset can hit the same validator / SC fault. The bridge just
> shouldn't be in the blast radius.

## The safe path — `McpDirectPackageSave`

A raw, SC-free, validation-free save. This is the canonical UE5 "save a package from
code" pattern (see Epic docs for `UPackage::Save`):

```cpp
UPackage* Package = Asset->GetOutermost();
// …reject /Temp, /Transient, RF_Transient…
Package->FullyLoad();                       // no-op if loaded; guards partial-load data loss

FSavePackageArgs SaveArgs;
SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
SaveArgs.SaveFlags     = SAVE_NoError;
SaveArgs.Error         = GError;

FSavePackageResultStruct Result =
    UPackage::Save(Package, nullptr, *PackageFileName, SaveArgs);
return Result.IsSuccessful();
```

Caller is responsible for the in-memory edit first, in this order:
`Asset->Modify()` → mutate the property → `Asset->PostEditChangeProperty(...)` →
`Asset->MarkPackageDirty()` → `McpDirectPackageSave(Asset, &OutError)`.

## The source-control caveat — read-only files

Skipping the editor's checkout step means we never auto-check-out the file. That is
**fine for Git** (it doesn't lock files; the write succeeds and the file simply shows as
modified). It is **not** fine for systems that mark unchecked-out files read-only on disk
(**Perforce**, Plastic): a raw `UPackage::Save` to a read-only file fails — and would
otherwise fail *silently* (just `saved: false`).

`McpDirectPackageSave` therefore checks for a read-only target before saving and reports
a clear, actionable error via its `OutError` out-param:

```
Package file is read-only on disk: '<path>'. It is likely locked by source control
(e.g. Perforce) — check it out before saving.
```

Handlers should surface that to the caller (e.g. `ASSET_SAVE_FAILED`) rather than
reporting a successful set with `saved: false`.

## See also
- `Private/McpSafeOperations.h` — `McpSafeAssetSave`, `McpDirectPackageSave`, `McpSafeLevelSave`
- `Private/McpAutomationBridge_AssetWorkflowHandlers.cpp` — `HandleSetAssetProperty`
  (`McpDirectPackageSave` reference caller); `HandleSaveAssetStatic` (`McpSafeAssetSave` reference
  caller — backs `manage_asset` `save`/`save_asset`, never force-loads an unloaded package);
  `manage_asset` `save_all` reuses `control_editor`'s save-all handler
- AGENTS.md → **UE SAFETY**
