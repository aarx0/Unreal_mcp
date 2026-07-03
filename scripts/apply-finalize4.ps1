<#
  apply-finalize4.ps1 — like v3, but also tolerates (and drops) a redundant
  `VAR->MarkPackageDirty();` line between the mark and the save. MarkBlueprintAs
  [Structurally]Modified already dirties the package, so the explicit call is a
  no-op; McpFinalizeBlueprint's mark covers it. Same strict safety as v3: only
  blank / //comment / the redundant MarkPackageDirty / a single same-var compile
  may sit between mark and save; any real code line breaks the match.
#>
param([Parameter(Mandatory)][string] $File)
$ErrorActionPreference = 'Stop'
$content = [System.IO.File]::ReadAllText($File)

# interstitial: blank | //comment | redundant `<var>->MarkPackageDirty();`  (var = backref \3)
$fill = '(?:\r?\n[ \t]*(?://[^\r\n]*)?|\r?\n[ \t]*\3->MarkPackageDirty\(\);)*'
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
  "$($m.Groups[1].Value)$($m.Groups[5].Value)McpFinalizeBlueprint($($m.Groups[3].Value), /*bStructural=*/$(if ($m.Groups[2].Success) {'true'} else {'false'}), /*bSave=*/true);"
}
$new = [regex]::Replace($content, $pattern, $evaluator)
if ($count -gt 0 -and $new -ne $content) { [System.IO.File]::WriteAllText($File, $new) }
Write-Host ("{0}: collapsed {1} (MarkPackageDirty-incl) finalize tail(s)" -f (Split-Path $File -Leaf), $count)
