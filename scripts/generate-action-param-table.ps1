<#
.SYNOPSIS
  Generates the per-action param table (McpActionParamTable.h) from handler source.

  For each handler file, action-comparison branch markers (SubAction == TEXT("x"),
  Lower.Equals(TEXT("y")), ...) segment the file; payload reads between markers are
  attributed to that action (consecutive markers with no reads between them form an
  alias group and share attribution). Reads before the first marker are file-shared:
  every action dispatched in that file accepts them. Reads in files with no markers
  (shared helpers, function-per-action handlers) are global-shared. Tool -> action
  truth comes from the checked-in schema golden (tests/schema/tools-schema.golden.json).

  The output header is consumed by the transport's per-action unknown-param check.
  tests/schema/action-param-table-test.ps1 regenerates and diffs it so the checked-in
  table cannot drift from handler source.

  v1 limitations (all fail PERMISSIVE - a missed attribution can only suppress a
  warning, never invent one): function-per-action handler files contribute their
  reads as global-shared; keys read through variables are invisible (same as the
  reconciliation test); multi-line comparisons whose TEXT lands on the next line
  are not markers.

.PARAMETER OutFile
  Where to write the generated header (default: the canonical in-tree location).
#>
param(
  [string]$OutFile = (Join-Path $PSScriptRoot '../plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/McpActionParamTable.h')
)
$ErrorActionPreference = 'Stop'

$srcRoot    = Resolve-Path (Join-Path $PSScriptRoot '../plugins/McpAutomationBridge/Source/McpAutomationBridge/Private')
$goldenPath = Resolve-Path (Join-Path $PSScriptRoot '../tests/schema/tools-schema.golden.json')

# --- tool -> action set, from the golden schema (authoritative published contract) ---
$golden = Get-Content $goldenPath -Raw | ConvertFrom-Json
$toolActions = [ordered]@{}
$allActions = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
foreach ($tool in $golden) {
  $enum = $tool.inputSchema.properties.action.enum
  if ($enum) {
    $toolActions[$tool.name] = @($enum)
    foreach ($a in $enum) { [void]$allActions.Add($a) }
  }
}
if ($allActions.Count -lt 100) { throw "golden parse produced only $($allActions.Count) actions - format change?" }

# --- read + marker regexes (read patterns shared with param-reconciliation-test.ps1) ---
# Receiver accepts Payload- AND Params-named variables: several handler
# families rename the request payload parameter (AnimationAuthoringHandlers
# uses `Params` — its reads were invisible until the 2026-07-03 spot-check
# caught set_bone_key's `frame` missing). Sub-object receivers that happen to
# end in Params over-attribute harmlessly (fail-permissive).
$readFn           = '(?:[A-Za-z0-9_]*(?:TryGet|Get|Has)[A-Za-z0-9_]*Field[A-Za-z0-9_]*|GetPayload[A-Za-z0-9_]+|Parse[A-Za-z0-9_]*FromJson)'
$payloadRecv      = '[A-Za-z0-9_]*(?:Payload|Params)'
$payloadReadRegex = [regex]('(?:\b' + $payloadRecv + '(?:\.Get\(\))?\s*->\s*' + $readFn + '(?:<[^<>]*>)?\s*\(\s*|\b' + $readFn + '\s*\(\s*' + $payloadRecv + '\s*,\s*)TEXT\("([^"]+)"\)')
$fallbackKeyRegex = [regex]('WithFallback\s*\(\s*' + $payloadRecv + '\s*[^";]{0,60}?TEXT\("[^"]+"\)\s*,\s*TEXT\("([^"]+)"\)')
# Action-comparison markers: an action-ish variable compared to a TEXT literal.
$markerRegex      = [regex]'\b(?:Lower[A-Za-z0-9_]*|[A-Za-z0-9_]*SubAction|[A-Za-z0-9_]*Action(?:Name|Lower|Str)?)\s*(?:==|\.Equals\s*\(|\.StartsWith\s*\()\s*TEXT\("([A-Za-z0-9_\-]+)"\)'

$handlerFiles = @(Get-ChildItem -Path $srcRoot -File | Where-Object { $_.Extension -in '.cpp', '.h' }) +
                @(Get-ChildItem -Path (Join-Path $srcRoot 'MCP') -Recurse -File |
                  Where-Object { $_.Extension -in '.cpp', '.h' -and $_.DirectoryName -notmatch '[\\/]Tools$' -and $_.Name -ne 'McpActionParamTable.h' })

$actionParams = [System.Collections.Generic.Dictionary[string, System.Collections.Generic.HashSet[string]]]::new([System.StringComparer]::Ordinal)
$fileSharedByAction = [System.Collections.Generic.Dictionary[string, System.Collections.Generic.HashSet[string]]]::new([System.StringComparer]::Ordinal)
$globalShared = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)

# Keys read through VARIABLES (statically invisible), curated by hand with the
# read site. Same exemption class as the reconciliation test's allowlist.
# - GASHandlers.cpp:539-541 alias loop: blueprintPath substitutes for every
#   manage_gas action.
foreach ($k in @('attributeSetPath', 'effectPath', 'cuePath')) { [void]$globalShared.Add($k) }

function Add-Param($dict, [string]$action, [string]$param) {
  if (-not $dict.ContainsKey($action)) { $dict[$action] = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal) }
  [void]$dict[$action].Add($param)
}

foreach ($f in $handlerFiles) {
  $text = Get-Content $f.FullName -Raw

  # Brace-scope attribution: a read belongs to the INNERMOST action branch
  # whose { ... } body encloses it; reads outside every branch body are
  # file-shared (accepted by all actions dispatched in this file). This keeps
  # early-gate patterns (an if-return branch at the top of the handler) from
  # swallowing the shared prologue reads that follow them.

  # One pass over braces -> position + running depth.
  $bracePos   = [System.Collections.Generic.List[int]]::new()
  $braceDepth = [System.Collections.Generic.List[int]]::new()
  $braceChar  = [System.Collections.Generic.List[char]]::new()
  $depth = 0
  for ($i = 0; $i -lt $text.Length; $i++) {
    $c = $text[$i]
    if ($c -eq '{') { $depth++; $bracePos.Add($i); $braceDepth.Add($depth); $braceChar.Add($c) }
    elseif ($c -eq '}') { $bracePos.Add($i); $braceDepth.Add($depth); $braceChar.Add($c); $depth-- }
  }

  # Branch ranges: marker -> body [openBrace, closeBrace]. Markers whose
  # condition shares one body (a == "x" || a == "y") produce identical ranges
  # and therefore share attribution.
  $ranges = @()   # pscustomobject: Start, End, Action
  $fileActions = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
  foreach ($m in $markerRegex.Matches($text)) {
    $name = $m.Groups[1].Value
    if (-not $allActions.Contains($name)) { continue }
    # First '{' after the marker = branch body open (search the brace index).
    $bi = 0
    while ($bi -lt $bracePos.Count -and ($bracePos[$bi] -lt $m.Index -or $braceChar[$bi] -ne '{')) { $bi++ }
    if ($bi -ge $bracePos.Count) { continue }
    $openPos = $bracePos[$bi]; $openDepth = $braceDepth[$bi]
    # Matching '}' = next brace at the same depth that is a closer.
    $ci = $bi + 1
    while ($ci -lt $bracePos.Count -and -not ($braceChar[$ci] -eq '}' -and $braceDepth[$ci] -eq $openDepth)) { $ci++ }
    if ($ci -ge $bracePos.Count) { continue }
    $ranges += [pscustomobject]@{ Start = $openPos; End = $bracePos[$ci]; Action = $name }
    [void]$fileActions.Add($name)
  }

  $reads = @()
  foreach ($m in $payloadReadRegex.Matches($text)) { $reads += [pscustomobject]@{ Index = $m.Index; Value = $m.Groups[1].Value } }
  foreach ($m in $fallbackKeyRegex.Matches($text)) { $reads += [pscustomobject]@{ Index = $m.Index; Value = $m.Groups[1].Value } }

  $fileShared = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
  foreach ($r in $reads) {
    # All branches enclosing this read, innermost = smallest span. Identical
    # spans (alias groups) all receive the read.
    $enclosing = @($ranges | Where-Object { $_.Start -lt $r.Index -and $r.Index -lt $_.End })
    if ($enclosing.Count -eq 0) { [void]$fileShared.Add($r.Value); continue }
    $minSpan = ($enclosing | ForEach-Object { $_.End - $_.Start } | Measure-Object -Minimum).Minimum
    foreach ($range in $enclosing | Where-Object { ($_.End - $_.Start) -eq $minSpan }) {
      Add-Param $actionParams $range.Action $r.Value
    }
  }

  if ($fileActions.Count -eq 0) {
    foreach ($p in $fileShared) { [void]$globalShared.Add($p) }
  }
  else {
    foreach ($a in $fileActions) {
      foreach ($p in $fileShared) { Add-Param $fileSharedByAction $a $p }
    }
  }
}

# --- compose per-tool.action sets: branch + file-shared ---
$entries = [System.Collections.Generic.List[string]]::new()
$entryRefs = [System.Collections.Generic.List[string]]::new()
$entryCount = 0
foreach ($tool in $toolActions.Keys) {
  foreach ($action in ($toolActions[$tool] | Sort-Object)) {
    $set = [System.Collections.Generic.SortedSet[string]]::new([System.StringComparer]::Ordinal)
    if ($actionParams.ContainsKey($action))       { foreach ($p in $actionParams[$action])       { [void]$set.Add($p) } }
    if ($fileSharedByAction.ContainsKey($action)) { foreach ($p in $fileSharedByAction[$action]) { [void]$set.Add($p) } }
    if ($set.Count -eq 0) { continue }  # no attribution -> no entry -> validator skips this action
    $quoted = @($set | ForEach-Object { "TEXT(`"$_`")" }) -join ', '
    # Row = key literal, param literals, nullptr terminator. Plain arrays of
    # string literals get true constant-initialization (no C4883 giant
    # dynamic-initializer function, unlike initializer_list aggregates).
    $entries.Add("inline const TCHAR* const GEntry_$entryCount[] = { TEXT(`"$tool.$action`"), $quoted, nullptr };")
    $entryRefs.Add("`tGEntry_$entryCount,")
    $entryCount++
  }
}
$globalQuoted = @($globalShared | Sort-Object | ForEach-Object { "TEXT(`"$_`")" }) -join ",`n`t`t`t"

$header = @"
// GENERATED FILE - do not hand-edit. Regenerate:
//   pwsh -File scripts/generate-action-param-table.ps1
// Freshness is enforced by tests/schema/action-param-table-test.ps1.
//
// Per-action accepted-param sets, statically attributed from handler source
// (branch-local payload reads + the handler file's shared prologue reads).
// GlobalSharedParams holds reads from files with no action branches (shared
// helpers, function-per-action handlers) - accepted for every action.
// Attribution is fail-permissive: a missing entry or param can only suppress
// a warning, never produce a false one.
#pragma once

#include "CoreMinimal.h"
#include "Containers/Map.h"
#include "Containers/Set.h"

namespace McpActionParams
{
inline const TSet<FString>& GlobalSharedParams()
{
	static const TSet<FString> Shared = {
			$globalQuoted
	};
	return Shared;
}

// Key: "<tool>.<action>". Returns nullptr when the pair has no attribution
// (validator must then skip the per-action check for that call).
// Rows are constant-initialized arrays of literals (key, params..., nullptr);
// the map builds from them on first use. initializer_list aggregates or one
// big Add() function trip C4883 at this entry count.
namespace Private
{
$($entries -join "`n")
inline const TCHAR* const* const GEntries[] =
{
$($entryRefs -join "`n")
};
}

inline const TSet<FString>* FindActionParams(const FString& Tool, const FString& Action)
{
	static const TMap<FString, TSet<FString>> Table = []()
	{
		TMap<FString, TSet<FString>> Built;
		Built.Reserve(UE_ARRAY_COUNT(Private::GEntries));
		for (const TCHAR* const* Row : Private::GEntries)
		{
			TSet<FString> Params;
			for (int32 i = 1; Row[i] != nullptr; ++i)
			{
				Params.Add(Row[i]);
			}
			Built.Add(Row[0], MoveTemp(Params));
		}
		return Built;
	}();
	return Table.Find(Tool + TEXT(".") + Action);
}
}
"@

Set-Content -Path $OutFile -Value $header -Encoding utf8
Write-Host "Generated $OutFile ($entryCount tool.action entries, $($globalShared.Count) global-shared params)"
