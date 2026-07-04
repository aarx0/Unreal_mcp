<#
.SYNOPSIS
  Regression test for manage_asset material-authoring value setters + compile read-backs.
  Composes existing bridge actions over MCP HTTP (no engine rebuild). Editor + bridge must be up.

  Covers:
    - set_node_value on Constant3Vector / ScalarParameter / arithmetic ConstA+ConstB, with
      get_node_properties read-back of each written value
    - set_node_value UNSUPPORTED_NODE error path (a non-settable node type)
    - get_material_info  -> reports `compiled` (+ errors[] on failure)
    - get_material_stats -> reports `compiled` + an honest instructionCountNote
    - compile_material    -> compiled:true on a valid material

  Exits non-zero if any assertion fails. Self-cleans the scratch asset even on failure.

.EXAMPLE
  pwsh -File tests/material/material-authoring-test.ps1
#>
param(
  [string]$Url = 'http://127.0.0.1:3123/mcp',
  [string]$ScratchPath = '/Game/VFX/Telegraph/M_ZZ_MatAuthTest',
  [string]$ScratchFolder = '/Game/VFX/Telegraph',
  [string]$ScratchName = 'M_ZZ_MatAuthTest',
  [string]$BadPath = '/Game/VFX/Telegraph/M_ZZ_MatAuthTestBad',
  [string]$BadName = 'M_ZZ_MatAuthTestBad'
)
$ErrorActionPreference = 'Stop'
$accept = 'application/json, text/event-stream'

# --- MCP session (one initialize, many tools/call) ---
$initBody = @{ jsonrpc='2.0'; id=1; method='initialize'; params=@{ protocolVersion='2025-06-18'; capabilities=@{}; clientInfo=@{ name='material-authoring-test'; version='1.0' } } } | ConvertTo-Json -Depth 12
$init = Invoke-WebRequest -Uri $Url -Method Post -Body $initBody -ContentType 'application/json' -Headers @{ Accept=$accept } -TimeoutSec 60
$sid = $init.Headers['Mcp-Session-Id']; if ($sid -is [array]) { $sid = $sid[0] }
if (-not $sid) { throw 'No Mcp-Session-Id from initialize' }
$script:cid = 1
function Mcp($tool, $a) {
  $script:cid++
  $body = @{ jsonrpc='2.0'; id=$script:cid; method='tools/call'; params=@{ name=$tool; arguments=$a } } | ConvertTo-Json -Depth 12
  $resp = Invoke-WebRequest -Uri $Url -Method Post -Body $body -ContentType 'application/json' -Headers @{ Accept=$accept; 'Mcp-Session-Id'=$sid } -TimeoutSec 180
  $j = $resp.Content; if ($j -match '^\s*data:\s*(.+)$') { $j = $Matches[1] }
  $envObj = $j | ConvertFrom-Json
  $inner = $envObj.result.content | Where-Object { $_.type -eq 'text' } | Select-Object -First 1
  $t = $inner.text; $i = $t.IndexOf("`n`n")
  $d = if ($i -ge 0) { $t.Substring($i + 2) } else { $t }
  try { return ($d | ConvertFrom-Json) } catch { return $d }
}

# --- assertions ---
$script:pass = 0; $script:fail = 0
function Assert($cond, $name) {
  if ($cond) { $script:pass++; Write-Host "  PASS  $name" }
  else       { $script:fail++; Write-Host "  FAIL  $name" -ForegroundColor Red }
}
function Near($a, $b) { return ([math]::Abs([double]$a - [double]$b) -lt 1e-3) }

try {
  # create
  $c = Mcp 'manage_asset' @{ action='create_material'; name=$ScratchName; path=$ScratchFolder }
  Assert ($c.existsAfter -eq $true) 'create_material existsAfter'

  # add nodes
  $c3 = Mcp 'manage_asset' @{ action='add_material_node'; assetPath=$ScratchPath; nodeType='Constant3Vector'; x=-400; y=0 }
  $sp = Mcp 'manage_asset' @{ action='add_material_node'; assetPath=$ScratchPath; nodeType='ScalarParameter'; name='TestScalar'; x=-400; y=200 }
  $ad = Mcp 'manage_asset' @{ action='add_material_node'; assetPath=$ScratchPath; nodeType='Add'; x=-400; y=400 }
  $tc = Mcp 'manage_asset' @{ action='add_material_node'; assetPath=$ScratchPath; nodeType='TextureCoordinate'; x=-700; y=0 }
  Assert ($c3.nodeId -and $sp.nodeId -and $ad.nodeId -and $tc.nodeId) 'add_material_node returns nodeIds'

  # set_node_value: Constant3Vector channels
  $r1 = Mcp 'manage_asset' @{ action='set_node_value'; assetPath=$ScratchPath; nodeId=$c3.nodeId; r=0.1; g=0.2; b=0.3 }
  Assert ($r1.applied -match 'R=0.1' -and $r1.applied -match 'G=0.2' -and $r1.applied -match 'B=0.3') 'set Constant3Vector applied'
  $p1 = Mcp 'manage_asset' @{ action='get_node_properties'; assetPath=$ScratchPath; nodeId=$c3.nodeId }
  Assert ($p1.properties.Constant -match 'R=0.100000' -and $p1.properties.Constant -match 'G=0.200000' -and $p1.properties.Constant -match 'B=0.300000') 'Constant3Vector read-back'

  # set_node_value: ScalarParameter default
  $r2 = Mcp 'manage_asset' @{ action='set_node_value'; assetPath=$ScratchPath; nodeId=$sp.nodeId; value=0.7 }
  Assert ($r2.applied -match 'DefaultValue=0.7') 'set ScalarParameter applied'
  $p2 = Mcp 'manage_asset' @{ action='get_node_properties'; assetPath=$ScratchPath; nodeId=$sp.nodeId }
  Assert (Near $p2.defaultValue 0.7) 'ScalarParameter read-back'

  # set_node_value: arithmetic ConstA/ConstB (reflection path)
  $r3 = Mcp 'manage_asset' @{ action='set_node_value'; assetPath=$ScratchPath; nodeId=$ad.nodeId; constA=1.5; constB=2.5 }
  Assert ($r3.applied -match 'ConstA=1.5' -and $r3.applied -match 'ConstB=2.5') 'set Add ConstA/ConstB applied'
  $p3 = Mcp 'manage_asset' @{ action='get_node_properties'; assetPath=$ScratchPath; nodeId=$ad.nodeId }
  Assert ((Near $p3.properties.ConstA 1.5) -and (Near $p3.properties.ConstB 2.5)) 'Add ConstA/ConstB read-back'

  # set_node_value: unsupported node -> error
  $r4 = Mcp 'manage_asset' @{ action='set_node_value'; assetPath=$ScratchPath; nodeId=$tc.nodeId; value=1 }
  Assert ("$r4" -match 'UNSUPPORTED_NODE') 'set_node_value error path on non-settable node'

  # read-back compile status on a valid material
  $gi = Mcp 'manage_asset' @{ action='get_material_info'; assetPath=$ScratchPath; filter='parameters' }
  Assert ($gi.compiled -eq $true) 'get_material_info reports compiled:true'
  Assert (-not $gi.errors) 'get_material_info has no errors[] on a valid material'

  $gs = Mcp 'manage_asset' @{ action='get_material_stats'; assetPath=$ScratchPath }
  Assert ($gs.stats.compiled -eq $true) 'get_material_stats reports compiled:true'
  Assert ([bool]$gs.stats.instructionCountNote) 'get_material_stats has instructionCountNote'

  $cm = Mcp 'manage_asset' @{ action='compile_material'; assetPath=$ScratchPath }
  Assert ($cm.compiled -eq $true) 'compile_material compiled:true'

  # compile_material waitForShaders catches a real shader-BACKEND error (forced synchronous
  # recompile) where the fast path is blind. Guaranteed-invalid HLSL in a Custom node -> EmissiveColor.
  Mcp 'manage_asset' @{ action='create_material'; name=$BadName; path=$ScratchFolder } | Out-Null
  $bc = Mcp 'manage_asset' @{ action='add_custom_expression'; assetPath=$BadPath; code='float qx = AAAA BBBB CCCC zzz; return float3(qx, qx, qx);'; outputType='Float3' }
  Mcp 'manage_asset' @{ action='connect_nodes'; assetPath=$BadPath; sourceNodeId=$bc.nodeId; targetNodeId='Main'; inputName='EmissiveColor' } | Out-Null
  $fast = Mcp 'manage_asset' @{ action='compile_material'; assetPath=$BadPath }
  Assert ($fast.compiled -eq $true) 'broken shader: fast path is blind (compiled:true without waitForShaders)'
  $wait = Mcp 'manage_asset' @{ action='compile_material'; assetPath=$BadPath; waitForShaders=$true }
  Assert ($wait.compiled -eq $false) 'broken shader: waitForShaders catches it (compiled:false)'
  Assert (@($wait.errors).Count -ge 1 -and "$($wait.errors)" -match 'error') 'broken shader: errors[] populated'
}
finally {
  $allGone = $true
  foreach ($p in @($ScratchPath, $BadPath)) {
    try { $d = Mcp 'manage_asset' @{ action='delete'; assetPath=$p }; if ($d.existsAfter -ne $false) { $allGone = $false } }
    catch { Write-Host "  WARN  cleanup failed for $p" -ForegroundColor Yellow; $allGone = $false }
  }
  Assert $allGone 'cleanup (scratch assets removed)'
}

Write-Host ""
Write-Host "$($script:pass) pass, $($script:fail) fail"
if ($script:fail -gt 0) { exit 1 } else { exit 0 }
