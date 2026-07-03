<#
  move-widget-family.ps1 — relocate a set of subaction blocks out of the
  monolithic HandleManageWidgetAuthoringAction dispatcher into a family
  sub-handler, byte-for-byte (pure line move + one ResultJson preamble line).

  A "block" = the lines from a 4-indent `if (SubAction.Equals(TEXT("name"))...`
  marker through its matching 4-indent `}`. Blocks are assigned to a family by
  the subaction name(s) in their condition.

  Verification is the caller's job: git diff (content identical, just moved) +
  compile + smoke. This script also self-checks that the multiset of code lines
  is preserved (only the preamble line is added).
#>
param(
  [Parameter(Mandatory)] [string]   $File,
  [Parameter(Mandatory)] [string]   $FamilyFunc,    # e.g. HandleWidgetAuthoring_Style
  [Parameter(Mandatory)] [string[]] $Subactions     # primary subaction names to move
)

$ErrorActionPreference = 'Stop'
$lines = [System.Collections.Generic.List[string]](Get-Content -LiteralPath $File)
$n = $lines.Count

# --- locate all block-start markers (4-indent `if (SubAction.Equals`) ---
$starts = @()
for ($i = 0; $i -lt $n; $i++) {
  if ($lines[$i] -match '^    if \(SubAction\.Equals\(TEXT\(') { $starts += $i }
}
if ($starts.Count -eq 0) { throw "no block-start markers found" }

# --- build block descriptors: start, openBrace, end (matching 4-indent close), names ---
$blocks = @()
foreach ($s in $starts) {
  # opening brace: first `^    {` at or after start
  $ob = $s
  while ($ob -lt $n -and $lines[$ob] -notmatch '^    \{') { $ob++ }
  if ($ob -ge $n) { throw "no opening brace for block at line $($s+1)" }
  # close: first `^    }` after the opening brace
  $e = $ob + 1
  while ($e -lt $n -and $lines[$e] -notmatch '^    \}') { $e++ }
  if ($e -ge $n) { throw "no closing brace for block at line $($s+1)" }
  # names: every TEXT("...") in the condition (start..openBrace-1)
  $names = @()
  for ($k = $s; $k -lt $ob; $k++) {
    foreach ($m in [regex]::Matches($lines[$k], 'SubAction\.Equals\(TEXT\("([^"]+)"\)')) {
      $names += $m.Groups[1].Value
    }
  }
  $blocks += [pscustomobject]@{ Start=$s; End=$e; Names=$names }
}

# --- select blocks belonging to this family ---
$wanted = [System.Collections.Generic.HashSet[string]]::new()
foreach ($a in $Subactions) { [void]$wanted.Add($a) }
$selected = @()
foreach ($b in $blocks) {
  $hit = $false
  foreach ($nm in $b.Names) { if ($wanted.Contains($nm)) { $hit = $true } }
  if ($hit) {
    # sanity: a selected block's names must ALL be in the wanted set (no stray sibling)
    foreach ($nm in $b.Names) {
      if (-not $wanted.Contains($nm)) { throw "block at line $($b.Start+1) mixes wanted + unwanted subaction '$nm' — refusing" }
    }
    $selected += $b
  }
}
# every requested subaction must be found
$found = [System.Collections.Generic.HashSet[string]]::new()
foreach ($b in $selected) { foreach ($nm in $b.Names) { [void]$found.Add($nm) } }
foreach ($a in $Subactions) { if (-not $found.Contains($a)) { throw "requested subaction '$a' not found in any block" } }
if ($selected.Count -eq 0) { throw "no blocks selected" }

Write-Host "Selected $($selected.Count) block(s) for $FamilyFunc :"
foreach ($b in ($selected | Sort-Object Start)) {
  Write-Host ("  lines {0}-{1}  [{2}]" -f ($b.Start+1), ($b.End+1), ($b.Names -join ', '))
}

# --- gather moved text (original order) and mark source lines for removal ---
$remove = [bool[]]::new($n)
$moved = [System.Collections.Generic.List[string]]::new()
foreach ($b in ($selected | Sort-Object Start)) {
  for ($k = $b.Start; $k -le $b.End; $k++) { $moved.Add($lines[$k]); $remove[$k] = $true }
  $moved.Add('')   # blank line between moved blocks
}

# --- locate the family function: its signature, then its `    return false;` ---
$sig = -1
for ($i = 0; $i -lt $n; $i++) {
  if ($lines[$i] -match ("^bool UMcpAutomationBridgeSubsystem::" + [regex]::Escape($FamilyFunc) + "\(")) { $sig = $i; break }
}
if ($sig -lt 0) { throw "family function $FamilyFunc not found" }
# its opening brace, then its return false (the stub body)
$fob = $sig
while ($fob -lt $n -and $lines[$fob] -notmatch '^\{') { $fob++ }
$fret = $fob
while ($fret -lt $n -and $lines[$fret] -notmatch '^    return false;') { $fret++ }
if ($fret -ge $n) { throw "family function $FamilyFunc has no 'return false;' line" }
$hasPreamble = $false
for ($k = $fob; $k -lt $fret; $k++) { if ($lines[$k] -match 'TSharedPtr<FJsonObject> ResultJson =') { $hasPreamble = $true } }
# Only declare the shared ResultJson if a moved block actually uses it (else it
# would be an unused local -> -Wunused error). Blocks with their own `Result`
# local need no preamble.
$usesResultJson = $false
foreach ($ml in $moved) { if ($ml -match '\bResultJson\b') { $usesResultJson = $true; break } }
$needPreamble = $usesResultJson -and (-not $hasPreamble)

# --- rebuild file: skip removed source lines; at $fret insert preamble + moved blocks ---
$out = [System.Collections.Generic.List[string]]::new()
for ($i = 0; $i -lt $n; $i++) {
  if ($i -eq $fret) {
    if ($needPreamble) { $out.Add('    TSharedPtr<FJsonObject> ResultJson = McpHandlerUtils::CreateResultObject();') }
    foreach ($ml in $moved) { $out.Add($ml) }
  }
  if ($remove[$i]) { continue }
  $out.Add($lines[$i])
}

# --- self-check: code-line multiset preserved (only preamble added) ---
$before = ($lines | Where-Object { $_.Trim() -ne '' } | Sort-Object)
$after  = ($out   | Where-Object { $_.Trim() -ne '' } | Sort-Object)
$added   = (Compare-Object $before $after | Where-Object SideIndicator -eq '=>')
$dropped = (Compare-Object $before $after | Where-Object SideIndicator -eq '<=')
$okAdded = ($added.Count -eq 0) -or (($added.Count -eq 1) -and ($added[0].InputObject -match 'ResultJson = McpHandlerUtils::CreateResultObject'))
if (-not $okAdded -or $dropped.Count -ne 0) {
  Write-Host "SELF-CHECK FAILED — content not preserved:" -ForegroundColor Red
  Write-Host "  added:";   $added   | ForEach-Object { "    + " + $_.InputObject }
  Write-Host "  dropped:"; $dropped | ForEach-Object { "    - " + $_.InputObject }
  throw "aborting without writing"
}

Set-Content -LiteralPath $File -Value $out
Write-Host "OK — moved $($selected.Count) block(s) into $FamilyFunc; self-check passed (code lines preserved)." -ForegroundColor Green
