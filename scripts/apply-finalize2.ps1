<#
  apply-finalize2.ps1 — collapse an ADJACENT Blueprint finalize tail into
  McpFinalizeBlueprint. Handles the dominant cross-file shape:

      FBlueprintEditorUtils::MarkBlueprintAs[Structurally]Modified(BP);
     [McpSafeCompileBlueprint(BP);]                 # optional
     [const bool VAR = ]McpSafeAssetSave|SaveLoadedAssetThrottled(BP);

  -> [const bool VAR = ]McpFinalizeBlueprint(BP, /*bStructural=*/<bool>, /*bSave=*/true);

  STRICT + SAFE by construction:
   - mark / (compile) / save must be on CONSECUTIVE lines (only whitespace
     indentation differs) — no intervening logic, comments, or conditionals,
     so nothing else is disturbed.
   - the compile var and save var must BACKREFERENCE-match the mark var.
   - an optional `const bool VAR =` capture on the save is preserved (the helper
     returns the same save bool), so callers that report "saved" still compile.
   - mark-only sites (no adjacent save) and conditional / comment-separated /
     MarkPackageDirty sites do NOT match and are left for manual handling.
  Verify per file: git diff (pure tail collapse) + compile + smoke.
#>
param([Parameter(Mandatory)][string] $File)
$ErrorActionPreference = 'Stop'
$content = [System.IO.File]::ReadAllText($File)

$save = '(?:McpSafeOperations::)?(?:McpSafeAssetSave|SaveLoadedAssetThrottled)'
$pattern =
  '(?m)^([ \t]*)FBlueprintEditorUtils::MarkBlueprintAs(Structurally)?Modified\((\w+)\);' +
  '(?:\r?\n[ \t]*McpSafeCompileBlueprint\(\3\);)?' +
  '\r?\n([ \t]*)(const bool \w+ = )?' + $save + '\(\3\);'

$count = 0
$evaluator = {
  param($m)
  $script:count++
  $indent  = $m.Groups[1].Value
  $struct  = if ($m.Groups[2].Success) { 'true' } else { 'false' }
  $var     = $m.Groups[3].Value
  $capture = $m.Groups[5].Value   # "const bool X = " or ""
  "${indent}${capture}McpFinalizeBlueprint($var, /*bStructural=*/$struct, /*bSave=*/true);"
}
$new = [regex]::Replace($content, $pattern, $evaluator)
if ($count -gt 0 -and $new -ne $content) { [System.IO.File]::WriteAllText($File, $new) }
Write-Host ("{0}: collapsed {1} adjacent finalize tail(s)" -f (Split-Path $File -Leaf), $count)
