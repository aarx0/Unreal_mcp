<#
.SYNOPSIS
  Handler-source vs action-declaration lint (offline; no editor).

  The declarations (Private/MCP/Decls/McpDecl_*.h) are the AUTHORED contract —
  the server's "here's what I know how to do". This lint statically re-derives
  branch-local payload reads from the handler source (action-comparison markers
  + brace-scope attribution — the machinery that used to GENERATE the old
  parsed table, demoted to a drift detector) and fails when a handler branch
  reads a param its action's declaration doesn't list: the rot vector where a
  handler edit adds a read and nobody updates the declaration.

  One-directional by design: declared-but-never-parsed params are NOT flagged
  (the declarations were fleet-authored from full code reading and legitimately
  contain reads this regex parser cannot see — alias loops, renamed payload
  vars, cross-file dispatch). Deliberate exemptions live in
  action-decl-lint-allowlist.txt as `tool.action.param` lines.

.PARAMETER UpdateAllowlist
  Re-pin the allowlist from current findings instead of comparing.
#>
param(
  [switch]$UpdateAllowlist
)
$ErrorActionPreference = 'Stop'

$srcRoot    = Resolve-Path (Join-Path $PSScriptRoot '../../plugins/McpAutomationBridge/Source/McpAutomationBridge/Private')
$declsDir   = Join-Path $srcRoot 'MCP/Decls'
$goldenPath = Resolve-Path (Join-Path $PSScriptRoot 'tools-schema.golden.json')
$allowFile  = Join-Path $PSScriptRoot 'action-decl-lint-allowlist.txt'

# --- declarations: "tool.action" -> param set ---
$declParams = @{}
$unverified = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
foreach ($f in Get-ChildItem $declsDir -Filter 'McpDecl_*.h') {
  $text = Get-Content $f.FullName -Raw
  $arrays = @{}
  foreach ($m in [regex]::Matches($text, 'inline const FMcpParamDecl (\w+)\[\] = \{(.*?)\};')) {
    $arrays[$m.Groups[1].Value] = @([regex]::Matches($m.Groups[2].Value, '\{ TEXT\("([^"]+)"\)') | ForEach-Object { $_.Groups[1].Value })
  }
  foreach ($m in [regex]::Matches($text, '\{ TEXT\("([^"]+)"\), TEXT\("([^"]+)"\), (\w+|\{\})\s*,\s*(EMcpCallFlags::\w+) \}')) {
    $key = "$($m.Groups[1].Value).$($m.Groups[2].Value)"
    $arrRef = $m.Groups[3].Value
    $declParams[$key] = if ($arrays.ContainsKey($arrRef)) { $arrays[$arrRef] } else { @() }
    if ($m.Groups[4].Value -eq 'EMcpCallFlags::UnverifiedDecl') { [void]$unverified.Add($key) }
  }
}
if ($declParams.Count -lt 500) { throw "decl parse produced only $($declParams.Count) entries - format change?" }

# --- golden: action name -> tools (for attribution) ---
$golden = Get-Content $goldenPath -Raw | ConvertFrom-Json
$toolOfAction = @{}
foreach ($tool in $golden) {
  $enum = $tool.inputSchema.properties.action.enum
  if ($enum) { foreach ($a in $enum) { if (-not $toolOfAction.ContainsKey($a)) { $toolOfAction[$a] = @() }; $toolOfAction[$a] += $tool.name } }
}
$allActions = [System.Collections.Generic.HashSet[string]]::new([string[]]@($toolOfAction.Keys), [System.StringComparer]::Ordinal)

# --- parse handler source: BRANCH-LOCAL reads only (file-shared attribution was
#     the old table's slop source; the lint keeps the high-precision subset) ---
$readFn           = '(?:[A-Za-z0-9_]*(?:TryGet|Get|Has)[A-Za-z0-9_]*Field[A-Za-z0-9_]*|GetPayload[A-Za-z0-9_]+|Parse[A-Za-z0-9_]*FromJson)'
$payloadRecv      = '[A-Za-z0-9_]*(?:Payload|Params)'
$payloadReadRegex = [regex]('(?:\b' + $payloadRecv + '(?:\.Get\(\))?\s*->\s*' + $readFn + '(?:<[^<>]*>)?\s*\(\s*|\b' + $readFn + '\s*\(\s*' + $payloadRecv + '\s*,\s*)TEXT\("([^"]+)"\)')
$markerRegex      = [regex]'\b(?:Lower[A-Za-z0-9_]*|[A-Za-z0-9_]*SubAction|[A-Za-z0-9_]*Action(?:Name|Lower|Str)?)\s*(?:==|\.Equals\s*\(|\.StartsWith\s*\()\s*TEXT\("([A-Za-z0-9_\-]+)"\)'

$findings = [System.Collections.Generic.List[string]]::new()
$handlerFiles = @(Get-ChildItem -Path $srcRoot -Filter 'McpAutomationBridge_*.cpp')
foreach ($f in $handlerFiles) {
  $text = Get-Content $f.FullName -Raw

  $bracePos = [System.Collections.Generic.List[int]]::new()
  $braceDepth = [System.Collections.Generic.List[int]]::new()
  $braceChar = [System.Collections.Generic.List[char]]::new()
  $depth = 0
  for ($i = 0; $i -lt $text.Length; $i++) {
    $c = $text[$i]
    if ($c -eq '{') { $depth++; $bracePos.Add($i); $braceDepth.Add($depth); $braceChar.Add($c) }
    elseif ($c -eq '}') { $bracePos.Add($i); $braceDepth.Add($depth); $braceChar.Add($c); $depth-- }
  }

  $ranges = @()
  $affinity = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
  foreach ($m in $markerRegex.Matches($text)) {
    $name = $m.Groups[1].Value
    if (-not $allActions.Contains($name)) { continue }
    $bi = 0
    while ($bi -lt $bracePos.Count -and ($bracePos[$bi] -lt $m.Index -or $braceChar[$bi] -ne '{')) { $bi++ }
    if ($bi -ge $bracePos.Count) { continue }
    $openPos = $bracePos[$bi]; $openDepth = $braceDepth[$bi]
    $ci = $bi + 1
    while ($ci -lt $bracePos.Count -and -not ($braceChar[$ci] -eq '}' -and $braceDepth[$ci] -eq $openDepth)) { $ci++ }
    if ($ci -ge $bracePos.Count) { continue }
    $ranges += [pscustomobject]@{ Start = $openPos; End = $bracePos[$ci]; Action = $name }
    # Unambiguous actions (one owning tool) define this file's tool affinity.
    $owners = $toolOfAction[$name]
    if ($owners.Count -eq 1) { [void]$affinity.Add($owners[0]) }
  }

  foreach ($m in $payloadReadRegex.Matches($text)) {
    $param = $m.Groups[1].Value
    if ($param -in @('action', 'subAction', 'bypassParamCheck')) { continue }
    $enclosing = @($ranges | Where-Object { $_.Start -lt $m.Index -and $m.Index -lt $_.End })
    if ($enclosing.Count -eq 0) { continue }
    $minSpan = ($enclosing | ForEach-Object { $_.End - $_.Start } | Measure-Object -Minimum).Minimum
    foreach ($range in $enclosing | Where-Object { ($_.End - $_.Start) -eq $minSpan }) {
      # Multi-tool action names: only charge tools this FILE serves (affinity).
      # A name owned by several tools with no affinity signal is too ambiguous
      # to lint from this file.
      $owners = @($toolOfAction[$range.Action])
      $chargedTools = if ($owners.Count -eq 1) { $owners } else { @($owners | Where-Object { $affinity.Contains($_) }) }
      foreach ($tool in $chargedTools) {
        $key = "$tool.$($range.Action)"
        if ($unverified.Contains($key)) { continue }
        if (-not $declParams.ContainsKey($key)) { continue }
        if ($param -notin $declParams[$key]) {
          $line = 1 + ([regex]::Matches($text.Substring(0, $m.Index), "`n")).Count
          $findings.Add("$key.$param  ($($f.Name):$line)")
        }
      }
    }
  }
}
$findings = @($findings | Sort-Object -Unique)

if ($UpdateAllowlist) {
  $lines = @(
    '# Exemptions for tests/schema/action-decl-lint.ps1 (tool.action.param per line; # comments).'
    '# Regenerate: pwsh -File tests/schema/action-decl-lint.ps1 -UpdateAllowlist'
    '# An entry is either a read on a path the declaration deliberately excludes'
    '# (dead branch, alias resolved elsewhere) or a real decl gap to fix instead of pinning.'
    ''
  ) + @($findings | ForEach-Object { ($_ -split '  ')[0] })
  Set-Content -Path $allowFile -Value ($lines -join "`n") -Encoding utf8
  Write-Host "Allowlist updated: $allowFile ($($findings.Count) entries)"
  exit 0
}

$allow = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
if (Test-Path $allowFile) {
  foreach ($line in Get-Content $allowFile) {
    $entry = ($line -split '#', 2)[0].Trim()
    if ($entry) { [void]$allow.Add($entry) }
  }
}
$violations = @($findings | Where-Object { -not $allow.Contains(($_ -split '  ')[0]) })

if ($violations.Count -eq 0) {
  Write-Host "PASS  handler reads match action declarations ($($declParams.Count) decls, $($unverified.Count) unverified-skipped; allowlist $($allow.Count))"
  exit 0
}
Write-Host "FAIL  handler reads missing from declarations: $($violations.Count)" -ForegroundColor Red
$violations | Select-Object -First 500 | ForEach-Object { Write-Host "  $_" }
Write-Host '  fix: add the param to the action''s McpDecl_*.h entry, or pin with -UpdateAllowlist after review.'
exit 1
