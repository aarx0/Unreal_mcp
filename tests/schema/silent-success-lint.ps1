<#
.SYNOPSIS
  Silent-success convention lint (offline; no editor).

  A multi-optional-field "bag" setter (a UMcpAutomationBridgeSubsystem::Handle*
  member whose body has >= 2 `Payload->HasField(TEXT("..."))` guards) must honor
  the fail-in-place convention: emit NO_CHANGES_REQUESTED on an empty request and
  report applied/failed per field, never a hollow success. This lint enumerates
  every such handler and FAILS if one neither carries a convention marker
  (FMcpWriteReport / SendWriteReportResponse / NO_CHANGES_REQUESTED /
  PROPERTY_WRITE_FAILED / appliedProperties) nor is listed in
  silent-success-allowlist.txt with a justification.

  This is the forcing function for the silent-success migration: it stays RED
  until every bag setter is converted-or-justified, so the migration cannot
  silently stall (the recurring "we left it half-done" failure mode).

  The allowlist holds only JUSTIFIED NON-conversions — a heuristic false positive
  (a read-op / genuinely-atomic setter that happens to have >=2 HasField guards),
  NOT deferred work. Pinning an unconverted real bag setter defeats the point.

.PARAMETER ListConverted
  Also print the handlers already satisfying the convention (audit aid).
#>
param([switch]$ListConverted)
$ErrorActionPreference = 'Stop'

$srcRoot   = Resolve-Path (Join-Path $PSScriptRoot '../../plugins/McpAutomationBridge/Source/McpAutomationBridge/Private')
$allowFile = Join-Path $PSScriptRoot 'silent-success-allowlist.txt'

# Any of these in a handler body means it routes through the convention.
$convMarkers = @('FMcpWriteReport', 'SendWriteReportResponse', 'NO_CHANGES_REQUESTED',
                 'PROPERTY_WRITE_FAILED', 'appliedProperties')

$funcRegex = [regex]'bool\s+UMcpAutomationBridgeSubsystem::(Handle\w+)\s*\('
# A "bag setter" is an EDIT handler that writes an existing object's optional
# fields — identified by the setter verb in its NAME (Configure/Setup/Set<X>/
# Modify/Update/Adjust). Name-based, not read-based: the handlers use several
# read-helper families (GetJson*Field, GetPayload*, nested "properties" objects),
# so a read-pattern heuristic has blind spots (it silently missed all of
# manage_inventory). Names are uniform. This naturally excludes Create*/Add*/
# Get*/Query* and the *Action family dispatchers (not field-setters; default
# config on create is intended). Genuine misfits (a Setup* that actually creates;
# a generic single-property setter) go in the allowlist with a reason.
$setterNameRegex = [regex]'(?:Configure|Setup|Modify|Update|Adjust|Set[A-Z])'

# Among setter-named handlers, only MULTI-optional-field "bags" need the
# convention; a single-required-value atomic setter (SetVsync, SetCvar) already
# fails loud on a missing value and has no bag to partial-apply. An "optional
# read" spans read-helper families: HasField / TryGetObjectField (a nested bag) /
# a defaulted 3-arg Get*Field or GetPayload*. >= 2 of these == a bag setter.
$MinOptionalReads = 2
$optionalReadRegex = [regex](
  '(?:(?:Payload|LocalPayload|Params)\s*->\s*(?:HasField|TryGetObjectField)\s*\(\s*TEXT\("[^"]+"\))' +
  '|(?:Get(?:Json\w*Field|Payload\w+)\s*\(\s*(?:Payload|LocalPayload|Params)\s*,\s*TEXT\("[^"]+"\)\s*,)')

$converted   = [System.Collections.Generic.List[string]]::new()
$unconverted = [System.Collections.Generic.List[object]]::new()

foreach ($f in @(Get-ChildItem -Path $srcRoot -Filter 'McpAutomationBridge_*.cpp')) {
  $text = Get-Content $f.FullName -Raw
  foreach ($fm in $funcRegex.Matches($text)) {
    $fname = $fm.Groups[1].Value
    if ($fname -notmatch $setterNameRegex) { continue }
    # Body = brace-matched block starting at the first '{' after the signature.
    $openIdx = $text.IndexOf('{', $fm.Index)
    if ($openIdx -lt 0) { continue }
    $depth = 0; $endIdx = -1
    for ($i = $openIdx; $i -lt $text.Length; $i++) {
      $c = $text[$i]
      if ($c -eq '{') { $depth++ }
      elseif ($c -eq '}') { $depth--; if ($depth -eq 0) { $endIdx = $i; break } }
    }
    if ($endIdx -lt 0) { continue }
    $body = $text.Substring($openIdx, $endIdx - $openIdx + 1)

    if (@($optionalReadRegex.Matches($body)).Count -lt $MinOptionalReads) { continue }

    $isConv = $false
    foreach ($mk in $convMarkers) { if ($body.Contains($mk)) { $isConv = $true; break } }
    $line = 1 + ([regex]::Matches($text.Substring(0, $fm.Index), "`n")).Count
    if ($isConv) { $converted.Add("$fname  ($($f.Name):$line)") }
    else { $unconverted.Add([pscustomobject]@{ Name = $fname; Where = "$($f.Name):$line" }) }
  }
}

# --- allowlist (justified non-conversions; one Handle-name per line, # comments) ---
$allow = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
if (Test-Path $allowFile) {
  foreach ($line in Get-Content $allowFile) {
    $entry = ($line -split '#', 2)[0].Trim()
    if ($entry) { [void]$allow.Add($entry) }
  }
}

$violations = @($unconverted | Where-Object { -not $allow.Contains($_.Name) })
$total = $converted.Count + $unconverted.Count

if ($ListConverted) {
  Write-Host "Converted bag-setters ($($converted.Count)):"
  $converted | Sort-Object | ForEach-Object { Write-Host "  $_" }
}

if ($violations.Count -eq 0) {
  Write-Host "PASS  silent-success: $($converted.Count)/$total bag-setters converted; $($allow.Count) allowlisted, 0 unconverted."
  exit 0
}
Write-Host "FAIL  silent-success: $($violations.Count) bag-setter(s) not converted and not allowlisted (of $total total; $($converted.Count) converted)." -ForegroundColor Red
$violations | Sort-Object Name | ForEach-Object { Write-Host "  $($_.Name)  ($($_.Where))" }
Write-Host '  fix: convert to FMcpWriteReport + SendWriteReportResponse (NO_CHANGES_REQUESTED guard + applied/failed),'
Write-Host '       or, if a genuine false positive (read-op / atomic setter), add the Handle-name to silent-success-allowlist.txt with a reason.'
exit 1
