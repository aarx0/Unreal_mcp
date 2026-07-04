<#
.SYNOPSIS
  Freshness check for the generated per-action param table (offline; no editor).

  Regenerates McpActionParamTable.h from the current handler source + schema
  golden and diffs it against the checked-in copy. A mismatch means a handler
  gained/lost payload reads (or the golden changed) without the table being
  regenerated — the transport would then warn from stale attribution.

.PARAMETER Update
  Regenerate the checked-in table in place instead of comparing.

.EXAMPLE
  pwsh -File tests/schema/action-param-table-test.ps1          # check
  pwsh -File tests/schema/action-param-table-test.ps1 -Update  # re-pin
#>
param(
  [switch]$Update
)
$ErrorActionPreference = 'Stop'

$generator = Resolve-Path (Join-Path $PSScriptRoot '../../scripts/generate-action-param-table.ps1')
$checkedIn = Resolve-Path (Join-Path $PSScriptRoot '../../plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/McpActionParamTable.h')

if ($Update) {
  & pwsh -File $generator
  exit $LASTEXITCODE
}

$tmp = Join-Path ([System.IO.Path]::GetTempPath()) 'McpActionParamTable.actual.h'
& pwsh -File $generator -OutFile $tmp | Out-Null
if ($LASTEXITCODE -ne 0) { Write-Host 'FAIL  generator errored' -ForegroundColor Red; exit 1 }

$expected = (Get-Content $checkedIn -Raw) -replace "`r`n", "`n"
$actual   = (Get-Content $tmp -Raw) -replace "`r`n", "`n"
if ($expected -eq $actual) {
  $entries = ([regex]::Matches($expected, 'GEntry_\d+\[\]')).Count
  Write-Host "PASS  action-param table is fresh ($entries tool.action entries)"
  exit 0
}

Write-Host 'FAIL  checked-in action-param table differs from regenerated' -ForegroundColor Red
Write-Host "  actual written to: $tmp"
Write-Host "  diff hint: git diff --no-index `"$checkedIn`" `"$tmp`""
Write-Host '  if handler changes are intentional: re-run with -Update and commit the table.'
exit 1
