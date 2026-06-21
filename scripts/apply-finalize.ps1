<#
  apply-finalize.ps1 — collapse the hand-rolled Blueprint finalize tail
      FBlueprintEditorUtils::MarkBlueprintAs[Structurally]Modified(BP);
      <blank>
      if (GetPayloadBool(Payload, TEXT("save"), true)) { McpSafeAssetSave(BP); }
  into the single correct helper
      McpFinalizeBlueprint(BP, /*bStructural=*/<bool>, GetPayloadBool(Payload, TEXT("save"), true));
  which ALSO compiles before saving (the missing-compile / stale-CDO fix).

  Strict by construction: the save var must be the SAME identifier as the mark
  var (backreference), and the save condition must be exactly the GetPayloadBool
  "save" form. Any other shape (AssetCreated between, unconditional save, raw
  save without the if, an already-present compile) does NOT match and is left
  for manual handling. Reports the count; rely on git diff + compile + smoke.
#>
param([Parameter(Mandatory)][string] $File)
$ErrorActionPreference = 'Stop'
$content = [System.IO.File]::ReadAllText($File)

$pattern =
  '(?m)^([ \t]*)FBlueprintEditorUtils::MarkBlueprintAs(Structurally)?Modified\((\w+)\);' +
  '\r?\n[ \t]*\r?\n[ \t]*if \(GetPayloadBool\(Payload, TEXT\("save"\), true\)\) \{' +
  '\r?\n[ \t]*McpSafeAssetSave\(\3\);\r?\n[ \t]*\}'

$count = 0
$evaluator = {
  param($m)
  $script:count++
  $indent = $m.Groups[1].Value
  $struct = if ($m.Groups[2].Success) { 'true' } else { 'false' }
  $var    = $m.Groups[3].Value
  "${indent}McpFinalizeBlueprint($var, /*bStructural=*/$struct, GetPayloadBool(Payload, TEXT(`"save`"), true));"
}
$new = [regex]::Replace($content, $pattern, $evaluator)

if ($count -gt 0 -and $new -ne $content) {
  [System.IO.File]::WriteAllText($File, $new)
}
Write-Host ("{0}: collapsed {1} finalize tail(s)" -f (Split-Path $File -Leaf), $count)
