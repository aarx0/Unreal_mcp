<#
.SYNOPSIS
  Declarative CommonUI focus/nav regression runner over the MCP bridge (port 3123).

  Reads a spec (JSON), drives the bridge (play / open menu / nav), and asserts the
  runtime focus + activatable-stack + input state via `inspect ui_focus`. Prints
  pass/fail per assertion and exits non-zero if any assertion fails.

  No engine rebuild: it composes the existing canonical actions
  (control_editor play/stop/simulate_nav, control_actor call_actor_function,
  inspect ui_focus). This is the "recorded golden-path" layer described in
  COMMONUI_FOCUS_INPUT.md — checked in as data so a nav regression (a reordered
  widget, a swapped activatable, a menu that stops grabbing focus) is caught in CI
  instead of at QA / a console cert pass. Harden into C++ FAutomationTests later if
  wanted; the spec + assertions transfer.

.PARAMETER Spec
  Path to a spec JSON: { name, description?, steps: [ {do|assert ...} ] }.
  Step kinds:
    { "do": "play" } | { "do": "stop" } | { "do": "wait", "ms": N }
    { "do": "resolvePC" }                         resolve the PIE player controller name
    { "do": "call", "function": "TogglePause" }   call a BlueprintCallable fn on the PC
    { "do": "nav", "direction": "Down", "device": "gamepad" }
  Assert kinds (evaluated against a fresh ui_focus snapshot):
    { "assert": "inPie", "equals": true }
    { "assert": "inputType", "equals": "Gamepad" }
    { "assert": "stackContains", "value": "WBP_PauseScreen_C" }
    { "assert": "stackDepth", "equals": 2 }
    { "assert": "focusedWidget", "value": "SButton" }      slateType equals
    { "assert": "focusedWidgetNot", "value": "SViewport" } slateType not-equals (focus grabbed?)
    { "assert": "focusedName", "value": "Button_Resume" }  UMG name equals (the focused LEAF)
    { "assert": "focusPathContains", "value": "Button_Options" }  UMG name anywhere on the
        root->leaf focus path. Use for composite widgets (CommonButton / UserWidget rows)
        whose focused leaf is an unnamed internal slate widget.
  Any assert may carry a "note" (printed on failure, e.g. "expected-fail today").

.EXAMPLE
  ./ui-nav-test.ps1 -Spec ../tests/ui-nav/pause_menu.json
#>
param(
    [Parameter(Mandatory = $true)][string]$Spec,
    [string]$Url = 'http://127.0.0.1:3123/mcp',
    [int]$TimeoutSec = 60
)
$ErrorActionPreference = 'Stop'
$accept = 'application/json, text/event-stream'

# --- MCP session (initialize once, reuse the session id) ---
$initBody = @{ jsonrpc = '2.0'; id = 1; method = 'initialize'; params = @{ protocolVersion = '2025-06-18'; capabilities = @{}; clientInfo = @{ name = 'ui-nav-test'; version = '1' } } } | ConvertTo-Json -Depth 12
$init = Invoke-WebRequest -Uri $Url -Method Post -Body $initBody -ContentType 'application/json' -Headers @{ Accept = $accept } -TimeoutSec $TimeoutSec
$sid = $init.Headers['Mcp-Session-Id']; if ($sid -is [array]) { $sid = $sid[0] }
if (-not $sid) { throw 'No Mcp-Session-Id returned from initialize (is the editor + bridge running?)' }
$script:rpcId = 1

function Invoke-Tool {
    param([string]$Tool, [hashtable]$ToolArgs)
    $script:rpcId++
    $body = @{ jsonrpc = '2.0'; id = $script:rpcId; method = 'tools/call'; params = @{ name = $Tool; arguments = $ToolArgs } } | ConvertTo-Json -Depth 12
    $resp = Invoke-WebRequest -Uri $Url -Method Post -Body $body -ContentType 'application/json' -Headers @{ Accept = $accept; 'Mcp-Session-Id' = $sid } -TimeoutSec $TimeoutSec
    $raw = $resp.Content
    $txt = $raw
    if ($raw -match '^\s*data:\s*(.+)$') { $txt = $Matches[1] }
    $envObj = $txt | ConvertFrom-Json
    $inner = $envObj.result.content | Where-Object { $_.type -eq 'text' } | Select-Object -First 1
    $t = if ($inner) { $inner.text } else { $txt }
    $idx = $t.IndexOf("`n`n")
    $data = if ($idx -ge 0) { $t.Substring($idx + 2) } else { $t }
    return [pscustomobject]@{ text = $t; data = $data }
}

function Get-Focus { return ((Invoke-Tool 'inspect' @{ action = 'ui_focus' }).data | ConvertFrom-Json) }

# --- run spec ---
$specObj = Get-Content -Raw $Spec | ConvertFrom-Json
Write-Output "=== $($specObj.name) ==="
if ($specObj.description) { Write-Output $specObj.description }
$pcName = $null
$pass = 0; $fail = 0

foreach ($step in $specObj.steps) {
    if ($step.PSObject.Properties.Name -contains 'do') {
        switch ($step.do) {
            'play' { Invoke-Tool 'control_editor' @{ action = 'play' } | Out-Null; Write-Output '  > play' }
            'stop' { Invoke-Tool 'control_editor' @{ action = 'stop' } | Out-Null; Write-Output '  > stop' }
            'wait' { Start-Sleep -Milliseconds ([int]$step.ms); Write-Output "  > wait $($step.ms)ms" }
            'resolvePC' {
                $rep = Invoke-Tool 'inspect' @{ action = 'pie_report' }
                if ($rep.text -match '"name"\s*:\s*"([^"]*PlayerController[^"]*)"') { $pcName = $Matches[1]; Write-Output "  > resolved PC = $pcName" }
                else { throw 'resolvePC: could not find a *PlayerController* in pie_report' }
            }
            'call' {
                if (-not $pcName) { throw 'call: run resolvePC first' }
                Invoke-Tool 'control_actor' @{ action = 'call_actor_function'; actorName = $pcName; functionName = $step.function } | Out-Null
                Write-Output "  > call $($step.function) on $pcName"
            }
            'nav' {
                $a = @{ action = 'simulate_nav'; direction = $step.direction }
                if ($step.device) { $a.device = $step.device }
                Invoke-Tool 'control_editor' $a | Out-Null
                Write-Output "  > nav $($step.direction) ($($step.device))"
            }
            default { Write-Output "  ! unknown do: $($step.do)" }
        }
    }
    elseif ($step.PSObject.Properties.Name -contains 'assert') {
        $f = Get-Focus
        $props = $step.PSObject.Properties.Name
        $expected = if ($props -contains 'equals') { "$($step.equals)" } elseif ($props -contains 'value') { "$($step.value)" } else { '' }
        $ok = $false; $actual = $null
        switch ($step.assert) {
            'inPie' { $actual = $f.inPie; $ok = ($actual -eq [bool]$step.equals) }
            'inputType' { $actual = $f.inputType; $ok = ($actual -eq $step.equals) }
            'stackContains' { $actual = (($f.activatableStack | ForEach-Object { $_.class }) -join ','); $ok = ($f.activatableStack.class -contains $step.value) }
            'stackDepth' { $actual = @($f.activatableStack).Count; $ok = ($actual -eq [int]$step.equals) }
            'focusedWidget' { $actual = $f.focusedWidget.slateType; $ok = ($actual -eq $step.value) }
            'focusedWidgetNot' { $actual = $f.focusedWidget.slateType; $ok = ($null -ne $actual -and $actual -ne $step.value) }
            'focusedName' { $actual = $f.focusedWidget.name; $ok = ($actual -eq $step.value) }
            'focusPathContains' {
                $names = @($f.focusPath | ForEach-Object { $_.name } | Where-Object { $_ })
                $actual = $names -join '>'
                $ok = ($names -contains $step.value)
            }
            default { $actual = 'n/a'; $ok = $false }
        }
        if ($ok) { $pass++; Write-Output "  [PASS] $($step.assert) = '$actual'" }
        else {
            $fail++
            $line = "  [FAIL] $($step.assert): expected '$expected', got '$actual'"
            if ($step.note) { $line += "  ($($step.note))" }
            Write-Output $line
        }
    }
}
Write-Output ''
Write-Output "RESULT: $pass passed, $fail failed"
if ($fail -gt 0) { exit 1 } else { exit 0 }
