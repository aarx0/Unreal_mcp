<#
.SYNOPSIS
  Minimal MCP Streamable-HTTP client for the McpAutomationBridge (port 3123).
  Does the initialize handshake, then one tools/call, and prints the tool result.

.EXAMPLE
  ./mcp-call.ps1 -Tool system_control -Arguments @{ action='list_tests'; filter='McpBridge.SelfTest' }

.EXAMPLE
  ./mcp-call.ps1 -Tool system_control -Arguments @{ action='run_tests'; filter='McpBridge.SelfTest' }
#>
param(
    [string]$Tool = 'system_control',
    [Parameter(Mandatory = $true)][hashtable]$Arguments,
    [string]$Url = 'http://127.0.0.1:3123/mcp',
    [int]$TimeoutSec = 180
)
$ErrorActionPreference = 'Stop'
$accept = 'application/json, text/event-stream'

# 1) initialize -> session id in Mcp-Session-Id response header
$initBody = @{
    jsonrpc = '2.0'; id = 1; method = 'initialize'
    params  = @{ protocolVersion = '2025-06-18'; capabilities = @{}; clientInfo = @{ name = 'mcp-call.ps1'; version = '1.0' } }
} | ConvertTo-Json -Depth 12
$init = Invoke-WebRequest -Uri $Url -Method Post -Body $initBody -ContentType 'application/json' -Headers @{ Accept = $accept } -TimeoutSec $TimeoutSec
$sid = $init.Headers['Mcp-Session-Id']
if ($sid -is [array]) { $sid = $sid[0] }
if (-not $sid) { throw "No Mcp-Session-Id returned from initialize" }

# 2) tools/call
$callBody = @{
    jsonrpc = '2.0'; id = 2; method = 'tools/call'
    params  = @{ name = $Tool; arguments = $Arguments }
} | ConvertTo-Json -Depth 12
$resp = Invoke-WebRequest -Uri $Url -Method Post -Body $callBody -ContentType 'application/json' `
    -Headers @{ Accept = $accept; 'Mcp-Session-Id' = $sid } -TimeoutSec $TimeoutSec

# Body may be a JSON-RPC envelope or an SSE frame ("data: {...}"). Normalize.
$raw = $resp.Content
$jsonText = $raw
if ($raw -match '^\s*data:\s*(.+)$') { $jsonText = ($Matches[1]) }

try {
    $env = $jsonText | ConvertFrom-Json
    # Tool result text is "message\n\n{json data}" (see FMcpJsonRpc::BuildToolResult).
    $inner = $env.result.content | Where-Object { $_.type -eq 'text' } | Select-Object -First 1
    if ($inner -and $inner.text) {
        $txt = $inner.text
        # Emit just the JSON data portion (after the blank line) so callers can
        # ConvertFrom-Json directly; fall back to the whole text if none.
        $idx = $txt.IndexOf("`n`n")
        if ($idx -ge 0) { Write-Output $txt.Substring($idx + 2) }
        else { Write-Output $txt }
    }
    else {
        Write-Output $jsonText
    }
}
catch {
    Write-Output $raw
}
