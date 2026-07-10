<#
.SYNOPSIS
  Static schema-vs-handler param reconciliation test (offline; no editor).

  Parses the tool definitions (McpTool_*.cpp schema-builder calls) and the
  handler sources, then checks both drift directions:
    1. declared-but-never-read: a schema param no handler read consumes
       (dead param — the schema advertises something that does nothing).
    2. payload-read-but-undeclared: a key a handler reads directly off the
       request payload that no tool schema declares (undiscoverable param).
  Names in param-reconciliation-allowlist.txt are exempt from both checks.

  v1 limitation: matching is repo-global by NAME only — a param declared by
  one tool and read by a different tool's handler still reconciles, and
  nested schema keys share one namespace with top-level params.
  Keys read through a variable (e.g. alias-fallback loops) are invisible to
  the regexes; those params live in the allowlist.

.PARAMETER UpdateAllowlist
  Regenerate the allowlist from the current violations instead of comparing.

.EXAMPLE
  pwsh -File tests/schema/param-reconciliation-test.ps1                  # check
  pwsh -File tests/schema/param-reconciliation-test.ps1 -UpdateAllowlist # re-pin
#>
param(
  [switch]$UpdateAllowlist
)
$ErrorActionPreference = 'Stop'

$srcRoot    = Resolve-Path (Join-Path $PSScriptRoot '../../plugins/McpAutomationBridge/Source/McpAutomationBridge/Private')
# Tool facades + schema/registry infra moved upstream to the McpToolSchema
# module (Phase F2 split); scan it too for both decls and handler reads.
$schemaRoot = Resolve-Path (Join-Path $PSScriptRoot '../../plugins/McpAutomationBridge/Source/McpToolSchema')
$toolsDir   = Join-Path $schemaRoot 'Private/MCP/Tools'
$allowFile  = Join-Path $PSScriptRoot 'param-reconciliation-allowlist.txt'

# Schema-builder property declarations: .String(TEXT("name"), ...) etc.
$declRegex = [regex]'\.\s*(?:String|StringEnum|Number|Bool|Integer|Object|Array|ArrayOfObjects|FreeformObject)\s*\(\s*TEXT\("([^"]+)"\)'

# Read helpers: JsonObject methods (TryGet*/Get*/Has*Field), the GetJson*Field
# free helpers plus their per-file shadows/suffix variants (GetStringField,
# GetJsonNumberFieldNav, ...), the Extract*Field helpers (ExtractVectorField,
# ExtractRotatorField), and the GetPayload* aliases. The [^";] gap spans
# a receiver/first argument without escaping the statement.
$readFn           = '(?:[A-Za-z0-9_]*(?:TryGet|Get|Has|Extract)[A-Za-z0-9_]*Field[A-Za-z0-9_]*|GetPayload[A-Za-z0-9_]+|Parse[A-Za-z0-9_]*FromJson)'
$anyReadRegex     = [regex]('\b' + $readFn + '(?:<[^<>]*>)?\s*\(\s*[^";]{0,120}?TEXT\("([^"]+)"\)')
# Get*FieldWithFallback(Payload, primaryKey, fallbackKey, ...) reads two keys.
$fallbackKeyRegex = [regex]'WithFallback\s*\(\s*[^";]{0,80}?TEXT\("[^"]+"\)\s*,\s*TEXT\("([^"]+)"\)'
# Reads whose receiver/first argument is the top-level request payload.
$payloadReadRegex = [regex]('(?:\b[A-Za-z0-9_]*Payload(?:\.Get\(\))?\s*->\s*' + $readFn + '(?:<[^<>]*>)?\s*\(\s*|\b' + $readFn + '\s*\(\s*[A-Za-z0-9_]*Payload\s*,\s*)TEXT\("([^"]+)"\)')

# --- declared params: name -> declaring tool files ---
# Schema declarations live in the tool facades (McpTool_*.cpp) and, for
# schema-from-decls families, in their AppendSchema fragments (Calls/McpCalls_*.cpp
# — same builder DSL; the facade folds them at runtime). Scan both so a param
# authored in a fragment still counts as declared.
$declared = [System.Collections.Generic.Dictionary[string, object]]::new([System.StringComparer]::Ordinal)
$callsDir  = Join-Path $srcRoot 'MCP/Calls'
$toolFiles = @(Get-ChildItem -Path $toolsDir -Filter 'McpTool_*.cpp')
if (Test-Path $callsDir) { $toolFiles += @(Get-ChildItem -Path $callsDir -Filter 'McpCalls_*.cpp') }
if (-not $toolFiles) { throw "no McpTool_*.cpp found under $toolsDir" }
foreach ($f in $toolFiles) {
  $text = Get-Content $f.FullName -Raw
  foreach ($m in $declRegex.Matches($text)) {
    $name = $m.Groups[1].Value
    if (-not $declared.ContainsKey($name)) { $declared[$name] = [System.Collections.Generic.List[string]]::new() }
    if (-not $declared[$name].Contains($f.Name)) { $declared[$name].Add($f.Name) }
  }
}

# --- handler reads: any-receiver name set + payload-receiver name -> files ---
$handlerFiles = @(Get-ChildItem -Path $srcRoot -File | Where-Object { $_.Extension -in '.cpp', '.h' }) +
                @(Get-ChildItem -Path (Join-Path $srcRoot 'MCP') -Recurse -File |
                  Where-Object { $_.Extension -in '.cpp', '.h' -and $_.DirectoryName -notmatch '[\\/]Tools$' }) +
                @(Get-ChildItem -Path $schemaRoot -Recurse -File |
                  Where-Object { $_.Extension -in '.cpp', '.h' -and $_.DirectoryName -notmatch '[\\/]Tools$' })
if (-not $handlerFiles) { throw "no handler sources found under $srcRoot" }
$anyReads     = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
$payloadReads = [System.Collections.Generic.Dictionary[string, object]]::new([System.StringComparer]::Ordinal)
foreach ($f in $handlerFiles) {
  $text = Get-Content $f.FullName -Raw
  foreach ($m in $anyReadRegex.Matches($text))     { [void]$anyReads.Add($m.Groups[1].Value) }
  foreach ($m in $fallbackKeyRegex.Matches($text)) { [void]$anyReads.Add($m.Groups[1].Value) }
  foreach ($m in $payloadReadRegex.Matches($text)) {
    $name = $m.Groups[1].Value
    if (-not $payloadReads.ContainsKey($name)) { $payloadReads[$name] = [System.Collections.Generic.List[string]]::new() }
    if (-not $payloadReads[$name].Contains($f.Name)) { $payloadReads[$name].Add($f.Name) }
  }
}

$deadRaw       = @($declared.Keys     | Where-Object { -not $anyReads.Contains($_) }       | Sort-Object)
$undeclaredRaw = @($payloadReads.Keys | Where-Object { -not $declared.ContainsKey($_) }    | Sort-Object)

if ($UpdateAllowlist) {
  $lines = @(
    '# Exemptions for tests/schema/param-reconciliation-test.ps1 (one name per line, # comments).'
    '# Regenerate: pwsh -File tests/schema/param-reconciliation-test.ps1 -UpdateAllowlist'
    '# Review the diff before committing: a new entry is either an internal/indirectly-read'
    '# key or a real schema<->handler drift that should be fixed, not re-pinned.'
    ''
    '# -- declared in a tool schema, no handler read found --'
  )
  $lines += $deadRaw
  $lines += @('', '# -- read from the request payload, not declared in any tool schema --')
  $lines += $undeclaredRaw
  Set-Content -Path $allowFile -Value ($lines -join "`n") -Encoding utf8
  Write-Host "Allowlist updated: $allowFile ($($deadRaw.Count) dead + $($undeclaredRaw.Count) undeclared)"
  exit 0
}

if (-not (Test-Path $allowFile)) {
  Write-Host "FAIL  allowlist missing; run with -UpdateAllowlist to create it" -ForegroundColor Red
  exit 1
}
$allow = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
foreach ($line in Get-Content $allowFile) {
  $entry = ($line -split '#', 2)[0].Trim()
  if ($entry) { [void]$allow.Add($entry) }
}

$dead       = @($deadRaw       | Where-Object { -not $allow.Contains($_) })
$undeclared = @($undeclaredRaw | Where-Object { -not $allow.Contains($_) })
$stale      = @($allow | Where-Object { $_ -notin $deadRaw -and $_ -notin $undeclaredRaw } | Sort-Object)

if ($dead.Count -eq 0 -and $undeclared.Count -eq 0) {
  Write-Host "PASS  schema/handler param names reconcile ($($declared.get_Count()) declared, $($payloadReads.get_Count()) payload-read; allowlist $($allow.Count), stale $($stale.Count))"
  if ($stale.Count -gt 0) {
    Write-Host "  stale allowlist entries (no longer violations; prune or re-pin): $($stale -join ', ')"
  }
  exit 0
}

if ($dead.Count -gt 0) {
  Write-Host "FAIL  declared-but-never-read: $($dead.Count) param(s)" -ForegroundColor Red
  foreach ($p in $dead) { Write-Host ("  {0}  ({1})" -f $p, ($declared[$p] -join ', ')) }
  Write-Host "  fix: make the handler read it, or remove it from the tool schema."
}
if ($undeclared.Count -gt 0) {
  Write-Host "FAIL  payload-read-but-undeclared: $($undeclared.Count) key(s)" -ForegroundColor Red
  foreach ($p in $undeclared) { Write-Host ("  {0}  ({1})" -f $p, ($payloadReads[$p] -join ', ')) }
  Write-Host "  fix: declare it in the owning McpTool_*.cpp schema; allowlist only internal/nested keys."
}
exit 1
