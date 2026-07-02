<#
.SYNOPSIS
  Schema snapshot test: pins the published tools/list contract.

  Fetches tools/list from the live bridge, canonicalizes it (tools sorted by
  name, object keys sorted recursively), and diffs against the checked-in
  golden. Any tool added/removed, action enum change, param add/remove, or
  description edit fails the test until the golden is intentionally updated.

  This is the cheap insurance against the drift class where a schema edit or
  routing change silently alters what AI clients see (the schema IS the UX).

.PARAMETER UpdateGolden
  Regenerate the golden from the live bridge instead of comparing.

.EXAMPLE
  pwsh -File tests/schema/schema-snapshot-test.ps1              # compare
  pwsh -File tests/schema/schema-snapshot-test.ps1 -UpdateGolden # re-pin
#>
param(
  [string]$Url = 'http://127.0.0.1:3123/mcp',
  [switch]$UpdateGolden
)
$ErrorActionPreference = 'Stop'
$golden = Join-Path $PSScriptRoot 'tools-schema.golden.json'
$accept = 'application/json, text/event-stream'

# --- MCP session ---
$initBody = @{ jsonrpc='2.0'; id=1; method='initialize'; params=@{ protocolVersion='2025-06-18'; capabilities=@{}; clientInfo=@{ name='schema-snapshot-test'; version='1.0' } } } | ConvertTo-Json -Depth 12
$init = Invoke-WebRequest -Uri $Url -Method Post -Body $initBody -ContentType 'application/json' -Headers @{ Accept=$accept } -TimeoutSec 60
$sid = $init.Headers['Mcp-Session-Id']; if ($sid -is [array]) { $sid = $sid[0] }
if (-not $sid) { throw 'No Mcp-Session-Id from initialize' }

$listBody = @{ jsonrpc='2.0'; id=2; method='tools/list' } | ConvertTo-Json -Depth 4
$resp = Invoke-WebRequest -Uri $Url -Method Post -Body $listBody -ContentType 'application/json' -Headers @{ Accept=$accept; 'Mcp-Session-Id'=$sid } -TimeoutSec 60
$tools = ($resp.Content | ConvertFrom-Json).result.tools
if (-not $tools) { throw 'tools/list returned no tools' }

# --- canonicalize: sort tools by name, sort keys recursively ---
function Canonicalize($node) {
  if ($node -is [System.Management.Automation.PSCustomObject]) {
    $ordered = [ordered]@{}
    foreach ($p in ($node.PSObject.Properties.Name | Sort-Object)) {
      $ordered[$p] = Canonicalize $node.$p
    }
    return $ordered
  }
  if ($node -is [System.Collections.IEnumerable] -and $node -isnot [string]) {
    # ',' keeps arrays intact: a plain pipeline return unrolls single-element
    # arrays (required, one-value enums) into scalars — invalid JSON Schema.
    $items = @($node | ForEach-Object { ,(Canonicalize $_) })
    return ,$items
  }
  return $node
}

$canon = @($tools | Sort-Object -Property name | ForEach-Object { Canonicalize $_ })
$actual = $canon | ConvertTo-Json -Depth 32

if ($UpdateGolden) {
  Set-Content -Path $golden -Value $actual -Encoding utf8
  Write-Host "Golden updated: $golden ($($tools.Count) tools)"
  exit 0
}

if (-not (Test-Path $golden)) {
  Write-Host "FAIL  golden missing; run with -UpdateGolden to create it" -ForegroundColor Red
  exit 1
}

$expected = Get-Content -Path $golden -Raw
# Normalize line endings for the comparison
if ($actual.Replace("`r`n", "`n").TrimEnd() -eq $expected.Replace("`r`n", "`n").TrimEnd()) {
  Write-Host "PASS  published schema matches golden ($($tools.Count) tools)"
  exit 0
}

Write-Host "FAIL  published schema differs from golden" -ForegroundColor Red
$tmp = Join-Path ([System.IO.Path]::GetTempPath()) 'tools-schema.actual.json'
Set-Content -Path $tmp -Value $actual -Encoding utf8
Write-Host "  actual written to: $tmp"
Write-Host "  diff hint: git diff --no-index `"$golden`" `"$tmp`""
Write-Host "  if the change is intentional: re-run with -UpdateGolden and commit the golden."
exit 1
