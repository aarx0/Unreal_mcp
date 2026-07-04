<#
.SYNOPSIS
Offline docs-reference check: living docs must not reference things that no longer exist.

Three checks over every tracked .md except point-in-time records (CHANGELOGs, TODO.md,
dated audit/review reports):
  1. link     — relative markdown links resolve to a file in the repo
  2. path     — backticked file references (e.g. `Private/McpSafeOperations.h`) name a file
                that exists somewhere in the tree (basename match)
  3. symbol   — backticked Mcp*/Handle* identifiers appear somewhere in the source
  4. tripwire — known-rot phrases from past staleness audits (WebSocket endpoints, npx
                install steps, TS paths, dev-as-default-branch, ...)

A line that itself says the referent is gone (deleted/removed/legacy/moot/pruned/...) is
exempt — docs are allowed to describe history as history. Deliberate leftovers are pinned
in docs-reference-allowlist.txt (format: <doc-relpath>|<kind>|<token>).

.EXAMPLE
pwsh -File tests/docs/docs-reference-test.ps1                  # check
pwsh -File tests/docs/docs-reference-test.ps1 -UpdateAllowlist # pin current findings
#>
param([switch]$UpdateAllowlist)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$allowlistPath = Join-Path $PSScriptRoot 'docs-reference-allowlist.txt'

$allow = @{}
if (Test-Path $allowlistPath) {
    foreach ($l in Get-Content $allowlistPath) {
        $t = $l.Trim()
        if ($t -and -not $t.StartsWith('#')) { $allow[$t] = $true }
    }
}

# Point-in-time records: append-only or dated; exempt wholesale.
$skipDocs = @(
    '(^|/)CHANGELOG\.md$'
    '(^|/)TODO\.md$'
    '^docs/architecture-review-\d{4}'
    '^docs/dogfood-audit-\d{4}'
    '^docs/hardening-\d{4}'
    '^docs/dead-name-sweep-\d{4}'
    '^docs/transport-mid-call-drop-problem\.md$'
)

# A line may name a dead referent if it says it's dead (checked with one line of context
# either side — sentences wrap).
$negContext = "(?i)(deleted|removed|dropped|gone|dead|superseded|moot|legacy|unused|no longer|pruned|prunes|\bnot\b|don't|never |was |were |historical|upstream)"

$tripwires = @(
    @{ p = 'ws://127\.0\.0\.1';               why = 'WebSocket endpoint (transport deleted)' }
    @{ p = 'npx unreal-engine-mcp-server';    why = 'TS server install step (deleted)' }
    @{ p = '`src/tools/';                     why = 'TS source path (deleted)' }
    @{ p = 'TypeScript and C\+\+';            why = 'dual-implementation claim (TS deleted)' }
    @{ p = 'JSON-RPC 2\.0 \+ SSE';            why = 'SSE framing (de-streamed)' }
    @{ p = 'default branch `dev`';            why = 'main is the default branch (2026-07-03)' }
    @{ p = 'Saved/Config';                    why = 'settings live in Config/DefaultGame.ini (Saved/ is pruned)' }
)

$mdFiles = (git -C $repoRoot ls-files '*.md') -split "`n" | Where-Object { $_ }
$livingDocs = $mdFiles | Where-Object { $doc = $_; -not ($skipDocs | Where-Object { $doc -match $_ }) }

# Corpus for symbol lookups + basename set for path lookups.
$tracked = (git -C $repoRoot ls-files) -split "`n" | Where-Object { $_ }
$trackedLower = $tracked | ForEach-Object { $_.ToLowerInvariant() }
$basenames = @{}
foreach ($f in $tracked) { $basenames[([IO.Path]::GetFileName($f)).ToLowerInvariant()] = $true }

# A doc may reference a file by any suffix of its real path/name
# (`Build.cs` for McpAutomationBridge.Build.cs, `Private/MCP/McpJsonRpc.h`, ...).
function Test-TrackedSuffix([string]$tok) {
    $t = $tok.Replace('\', '/').ToLowerInvariant()
    if ($basenames.ContainsKey($t)) { return $true }
    foreach ($p in $trackedLower) {
        if ($p.EndsWith($t) -or ([IO.Path]::GetFileName($p)).EndsWith($t)) { return $true }
    }
    return $false
}
$sb = [System.Text.StringBuilder]::new()
foreach ($f in $tracked | Where-Object { $_ -match '\.(cpp|h|cs|ps1|psm1|py|json|ini|uplugin)$' }) {
    [void]$sb.Append((Get-Content (Join-Path $repoRoot $f) -Raw))
}
$corpus = $sb.ToString()

$findings = [System.Collections.Generic.List[object]]::new()
function Add-Finding($doc, $line, $kind, $token, $why) {
    $key = "$doc|$kind|$token"
    if (-not $allow.ContainsKey($key)) {
        $findings.Add([pscustomobject]@{ Doc = $doc; Line = $line; Kind = $kind; Token = $token; Why = $why; Key = $key })
    }
}

foreach ($doc in $livingDocs) {
    $docPath = Join-Path $repoRoot $doc
    $docDir = Split-Path $docPath -Parent
    $lines = Get-Content $docPath
    $inFence = $false
    for ($i = 0; $i -lt $lines.Count; $i++) {
        $line = $lines[$i]
        if ($line -match '^\s*```') { $inFence = -not $inFence; continue }
        $window = @($line)
        if ($i -gt 0) { $window += $lines[$i - 1] }
        if ($i -lt $lines.Count - 1) { $window += $lines[$i + 1] }
        $exempt = ($window -join ' ') -match $negContext

        # 1. relative markdown links
        foreach ($m in [regex]::Matches($line, '\[[^\]]*\]\(([^)#\s]+)(#[^)\s]*)?\)')) {
            $target = $m.Groups[1].Value
            if ($target -match '^(https?|mailto):') { continue }
            $resolved = [IO.Path]::GetFullPath((Join-Path $docDir $target))
            if (-not $resolved.StartsWith($repoRoot, [StringComparison]::OrdinalIgnoreCase)) { continue } # out-of-repo: not ours to guard
            if (-not (Test-Path $resolved)) { Add-Finding $doc ($i + 1) 'link' $target 'link target missing' }
        }

        if ($inFence -or $exempt) { continue }

        foreach ($m in [regex]::Matches($line, '`([^`]+)`')) {
            $tok = $m.Groups[1].Value.Trim()

            # 2. file-path tokens. Only OUR files are guarded: a reference is checked when
            # its basename says it's ours (Mcp*) or it's a .ts file (no TS exists in this
            # repo — any live .ts reference is rot). Engine/user-config paths are skipped.
            if ($tok -match '^[A-Za-z0-9_./\\-]+\.(md|cpp|h|ps1|cs|json|ini|uplugin|bat|sh|ts)$') {
                $base = [IO.Path]::GetFileName($tok)
                $ours = $base -match '^(?i)mcp[a-z_]' -or $tok -match '\.ts$' -or $tok -match '^(docs|tests|scripts)/'
                if ($ours -and -not (Test-TrackedSuffix $tok)) {
                    Add-Finding $doc ($i + 1) 'path' $tok 'no such file in tree'
                }
                continue
            }

            # 3. bridge symbols (Mcp*/Handle*, case-sensitive) must appear in source
            if ($tok -cmatch '^[A-Za-z_][A-Za-z0-9_:()]*$' -and $tok -cmatch '(Mcp|^Handle[A-Z])') {
                foreach ($part in ($tok -replace '\(.*\)$', '') -split '::') {
                    if ($part.Length -ge 4 -and -not $corpus.Contains($part)) {
                        Add-Finding $doc ($i + 1) 'symbol' $part 'not found in source'
                    }
                }
            }
        }

        # 4. tripwires
        foreach ($tw in $tripwires) {
            if ($line -match $tw.p) { Add-Finding $doc ($i + 1) 'tripwire' $tw.p $tw.why }
        }
    }
}

if ($UpdateAllowlist) {
    $header = @('# docs-reference-test allowlist — deliberate references to deleted/historical things.',
        '# Format: <doc-relpath>|<kind>|<token>. Each pin should be re-justified when its doc is edited.')
    $pins = $findings | ForEach-Object { $_.Key } | Sort-Object -Unique
    Set-Content -Path $allowlistPath -Value ($header + $pins) -Encoding utf8
    Write-Host "Pinned $($pins.Count) findings to $allowlistPath"
    exit 0
}

if ($findings.Count -eq 0) {
    Write-Host "docs-reference-test: PASS ($($livingDocs.Count) living docs, $($allow.Count) allowlist pins)" -ForegroundColor Green
    exit 0
}
Write-Host "docs-reference-test: FAIL — $($findings.Count) finding(s)" -ForegroundColor Red
$findings | ForEach-Object { Write-Host ("  {0}:{1} [{2}] {3} — {4}" -f $_.Doc, $_.Line, $_.Kind, $_.Token, $_.Why) }
Write-Host "`nFix the doc, or pin deliberate historical references: <doc>|<kind>|<token> in $allowlistPath"
exit 1
