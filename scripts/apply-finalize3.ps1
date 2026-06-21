<#
  apply-finalize3.ps1 — collapse the non-adjacent finalize tails v2 left:
  mark and save separated only by blank lines, save-narration // comments, and/
  or a compile call. Handles multi-line Mark(...) calls, either compile fn
  (McpSafeCompileBlueprint or FKismetEditorUtilities::CompileBlueprint), and a
  const/non-const/reassignment save capture.

      FBlueprintEditorUtils::MarkBlueprintAs[Structurally]Modified( <VAR> );
      <blank / // comment lines>
     [McpSafeCompileBlueprint(<VAR>); | FKismetEditorUtilities::CompileBlueprint(<VAR>);]
      <blank / // comment lines>
     [(const )?bool NAME = | NAME = ] (McpSafeAssetSave|SaveLoadedAssetThrottled)(<VAR>);

  -> [capture]McpFinalizeBlueprint(<VAR>, /*bStructural=*/<bool>, /*bSave=*/true);

  SAFE by construction: between the mark and the save ONLY blank lines, `//`
  comment lines, and a single (same-var) compile call may appear — ANY real
  code line breaks the match, so handler logic is never disturbed. The var is
  backreference-matched across mark/compile/save. Interstitial blank+comment
  lines (save-narration) are dropped. Conditional marks (two marks in an
  if/else) and mark-with-logic-between do NOT match and are left.
#>
param([Parameter(Mandatory)][string] $File)
$ErrorActionPreference = 'Stop'
$content = [System.IO.File]::ReadAllText($File)

# a run of zero or more blank-or-//comment lines
$fill = '(?:\r?\n[ \t]*(?://[^\r\n]*)?)*'
$compile = '(?:\r?\n[ \t]*(?:McpSafeCompileBlueprint|FKismetEditorUtilities::CompileBlueprint)\(\s*\3\s*\);)?'
$save = '(?:McpSafeOperations::)?(?:McpSafeAssetSave|SaveLoadedAssetThrottled)'
$pattern =
  '(?m)^([ \t]*)FBlueprintEditorUtils::MarkBlueprintAs(Structurally)?Modified\(\s*(\w+)\s*\);' +
  $fill + $compile + $fill +
  '\r?\n([ \t]*)((?:const\s+)?bool\s+\w+\s*=\s*|\w+\s*=\s*)?' + $save + '\(\s*\3\s*\);'

$count = 0
$evaluator = {
  param($m)
  $script:count++
  $indent  = $m.Groups[1].Value
  $struct  = if ($m.Groups[2].Success) { 'true' } else { 'false' }
  $var     = $m.Groups[3].Value
  $capture = $m.Groups[5].Value
  "${indent}${capture}McpFinalizeBlueprint($var, /*bStructural=*/$struct, /*bSave=*/true);"
}
$new = [regex]::Replace($content, $pattern, $evaluator)
if ($count -gt 0 -and $new -ne $content) { [System.IO.File]::WriteAllText($File, $new) }
Write-Host ("{0}: collapsed {1} non-adjacent finalize tail(s)" -f (Split-Path $File -Leaf), $count)
