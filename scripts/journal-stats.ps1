# Aggregates the bridge call journal (Saved/McpJournal/calls-*.jsonl) into
# failure/latency/param-usage stats. See docs/call-journal.md.
#
#   pwsh -File scripts/journal-stats.ps1                 # all days
#   pwsh -File scripts/journal-stats.ps1 -Day 20260711   # one day
#   pwsh -File scripts/journal-stats.ps1 -ArgUsage       # + arg-name frequency per tool
param(
    [string]$JournalDir = (Join-Path $PSScriptRoot "..\..\..\Saved\McpJournal"),
    [string]$Day,
    [switch]$ArgUsage
)

$pattern = if ($Day) { "calls-$Day.jsonl" } else { "calls-*.jsonl" }
$files = Get-ChildItem -Path $JournalDir -Filter $pattern -ErrorAction SilentlyContinue
if (-not $files) {
    Write-Error "No journal files matching '$pattern' in $JournalDir (journal starts with the first bridge call after the feature landed)"
    exit 1
}

$lines = $files | Get-Content | ForEach-Object { $_ | ConvertFrom-Json }
$total = $lines.Count
$fails = @($lines | Where-Object { -not $_.ok })
$okCalls = @($lines | Where-Object { $_.ok })

"files:  $($files.Name -join ', ')"
"calls:  $total   ok: $($okCalls.Count)   failed: $($fails.Count)  ($([math]::Round(100.0 * $fails.Count / [math]::Max($total,1), 1))%)"

if ($fails.Count -gt 0) {
    "`n--- failures by code / why ---"
    $fails | Group-Object -Property { "$($_.code) [$($_.why)]" } | Sort-Object Count -Descending |
        Format-Table Count, Name -AutoSize | Out-String | Write-Output

    "--- top failing tool.action ---"
    $fails | Group-Object -Property { "$($_.tool).$($_.action)" } | Sort-Object Count -Descending |
        Select-Object -First 15 | Format-Table Count, Name -AutoSize | Out-String | Write-Output

    "--- top offending params ---"
    $fails | ForEach-Object { $_.offending } | Where-Object { $_ } | Group-Object |
        Sort-Object Count -Descending | Select-Object -First 15 |
        Format-Table Count, Name -AutoSize | Out-String | Write-Output
}

"--- slowest actions (ok calls, by max ms) ---"
$okCalls | Group-Object -Property { "$($_.tool).$($_.action)" } | ForEach-Object {
    [pscustomobject]@{
        Action = $_.Name
        Calls  = $_.Count
        MeanMs = [math]::Round(($_.Group.ms | Measure-Object -Average).Average, 1)
        MaxMs  = [math]::Round(($_.Group.ms | Measure-Object -Maximum).Maximum, 1)
    }
} | Sort-Object MaxMs -Descending | Select-Object -First 10 | Format-Table -AutoSize | Out-String | Write-Output

if ($ArgUsage) {
    "--- arg-name usage per tool (alias-kill evidence) ---"
    $lines | Group-Object tool | Sort-Object Name | ForEach-Object {
        $tool = $_.Name
        $_.Group | ForEach-Object { $_.args } | Where-Object { $_ } | Group-Object |
            Sort-Object Count -Descending | ForEach-Object {
                [pscustomobject]@{ Tool = $tool; Arg = $_.Name; Count = $_.Count }
            }
    } | Format-Table -AutoSize | Out-String | Write-Output
}
