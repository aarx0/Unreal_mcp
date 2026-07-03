<#
  clean-widget-dispatcher.ps1 — after all families are migrated, tidy the now-
  thin HandleManageWidgetAuthoringAction: drop the unused dispatcher-scope
  ResultJson and the graveyard of orphaned section-divider comments left between
  the last family-dispatch call and the terminal "Action not recognized".

  Safety: refuses to remove any graveyard line that isn't blank or a // comment.
#>
param([Parameter(Mandatory)][string] $File)
$ErrorActionPreference = 'Stop'
$lines = [System.Collections.Generic.List[string]](Get-Content -LiteralPath $File)
$n = $lines.Count

# locate dispatcher
$disp = -1
for ($i=0; $i -lt $n; $i++) { if ($lines[$i] -match '^bool UMcpAutomationBridgeSubsystem::HandleManageWidgetAuthoringAction\(') { $disp=$i; break } }
if ($disp -lt 0) { throw "dispatcher not found" }

# the dispatcher's own ResultJson (first one at/after $disp)
$rj = -1
for ($i=$disp; $i -lt $n; $i++) { if ($lines[$i] -match '^    TSharedPtr<FJsonObject> ResultJson = McpHandlerUtils::CreateResultObject\(\);') { $rj=$i; break } }
if ($rj -lt 0) { throw "dispatcher ResultJson not found" }

# last family-dispatch call, then the terminal comment
$misc = -1
for ($i=$disp; $i -lt $n; $i++) { if ($lines[$i] -match '^    if \(HandleWidgetAuthoring_Misc\(') { $misc=$i; break } }
if ($misc -lt 0) { throw "Misc dispatch call not found" }
$term = -1
for ($i=$misc; $i -lt $n; $i++) { if ($lines[$i] -match '^    // Action not recognized') { $term=$i; break } }
if ($term -lt 0) { throw "terminal 'Action not recognized' not found" }

# graveyard = (misc+1 .. term-1); assert all blank-or-comment
for ($i=$misc+1; $i -lt $term; $i++) {
  $t = $lines[$i].Trim()
  if ($t -ne '' -and -not $t.StartsWith('//')) { throw "refusing: graveyard line $($i+1) is not blank/comment: $($lines[$i])" }
}

# build output
$out = [System.Collections.Generic.List[string]]::new()
for ($i=0; $i -lt $n; $i++) {
  if ($i -eq $rj) { continue }                       # drop unused ResultJson
  if ($i -eq $rj+1 -and $lines[$i].Trim() -eq '') { continue }  # and its trailing blank
  if ($i -gt $misc -and $i -lt $term) {              # collapse graveyard to one blank
    if ($i -eq $misc+1) { $out.Add('') }
    continue
  }
  $out.Add($lines[$i])
}
Set-Content -LiteralPath $File -Value $out
Write-Host "Cleaned dispatcher: removed unused ResultJson (line $($rj+1)) + $($term-$misc-1) graveyard line(s)." -ForegroundColor Green
