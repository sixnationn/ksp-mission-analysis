param(
    [string]$ManifestPath = (Join-Path $PSScriptRoot '../benchmarks/sessions.json'),
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '../benchmarks')
)
$ErrorActionPreference = 'Stop'
$manifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json
$rows = @()
$warnings = @()
$fields = @('input_tokens','cached_input_tokens','cache_write_input_tokens','output_tokens','reasoning_output_tokens','total_tokens')
foreach ($session in $manifest) {
    if (-not (Test-Path -LiteralPath $session.path)) {
        $warnings += "Missing log: $($session.path)"
        continue
    }
    $currentModel = 'unknown'
    $currentEffort = if ($session.reasoning_effort) { [string]$session.reasoning_effort } else { 'unknown' }
    $previous = @{}
    foreach ($field in $fields) { $previous[$field] = [long]0 }
    $buckets = @{}
    foreach ($line in (Get-Content -LiteralPath $session.path)) {
        try { $event = $line | ConvertFrom-Json } catch {
            $warnings += "Incomplete or malformed log line in $($session.path)"
            continue
        }
        if ($event.type -eq 'turn_context') {
            if ($event.payload.model) { $currentModel = [string]$event.payload.model }
            if ($event.payload.effort) { $currentEffort = [string]$event.payload.effort }
        }
        if ($event.type -ne 'event_msg' -or $event.payload.type -ne 'token_count') { continue }
        $total = $event.payload.info.total_token_usage
        if ($null -eq $total) { continue }
        $bucketKey = "$currentModel|$currentEffort"
        if (-not $buckets.ContainsKey($bucketKey)) {
            $buckets[$bucketKey] = [ordered]@{ role=$session.role; status=$session.status; model=$currentModel; reasoning_effort=$currentEffort; source=$session.path; as_of=$event.timestamp }
            foreach ($field in $fields) { $buckets[$bucketKey][$field] = [long]0 }
        }
        # Cumulative snapshots repeat: only add the increase. A reset is ambiguous,
        # so preserve prior totals and report it rather than guessing.
        if ([long]$total.total_tokens -lt $previous['total_tokens']) {
            $warnings += "Cumulative counter reset in $($session.path); later attribution incomplete"
            continue
        }
        foreach ($field in $fields) {
            $value = [long]$total.$field
            $buckets[$bucketKey][$field] += $value - $previous[$field]
            $previous[$field] = $value
        }
        $buckets[$bucketKey]['as_of'] = $event.timestamp
    }
    foreach ($bucket in $buckets.Values) {
        $bucket['uncached_input_tokens'] = $bucket['input_tokens'] - $bucket['cached_input_tokens']
        $rows += [pscustomobject]$bucket
    }
    if ($buckets.Count -eq 0) { $warnings += "No token counters: $($session.path)" }
}
$summaries = @($rows | Group-Object model,reasoning_effort | ForEach-Object {
    $summary = [ordered]@{model=$_.Group[0].model; reasoning_effort=$_.Group[0].reasoning_effort; sessions=$_.Count}
    foreach ($field in ($fields + 'uncached_input_tokens')) {
        $summary[$field] = [long](($_.Group | Measure-Object -Property $field -Sum).Sum)
    }
    [pscustomobject]$summary
})
$report = [ordered]@{
    generated_at_utc=[DateTime]::UtcNow.ToString('o')
    scope='Registered task sessions only; active response is incomplete until next refresh.'
    accounting='Input includes cached input. Reasoning is a subset of output. Total is input plus output; do not add subsets again. Uncached input includes any cache-write input; cache writes are also reported separately.'
    limitations='Recorded runtime counters, not an invoice or weekly-limit estimate. Model attribution follows turn_context. No timing or quality claim follows from token counts alone.'
    warnings=$warnings; models=$summaries; sessions=$rows
}
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'token-usage.json') -Encoding UTF8
$rows | Export-Csv -LiteralPath (Join-Path $OutputDirectory 'token-usage.csv') -NoTypeInformation -Encoding UTF8
$markdown = @('# Token benchmark', '', $report.scope, '', $report.accounting, '', $report.limitations, '', '| Model | Effort | Sessions | Fresh input | Cached input | Output | Reasoning subset | Total |', '|---|---|---:|---:|---:|---:|---:|---:|')
foreach ($row in $summaries) {
    $markdown += "| $($row.model) | $($row.reasoning_effort) | $($row.sessions) | $($row.uncached_input_tokens) | $($row.cached_input_tokens) | $($row.output_tokens) | $($row.reasoning_output_tokens) | $($row.total_tokens) |"
}
$markdown += @('', "Captured UTC: $($report.generated_at_utc)", '', 'See token-usage.csv for per-session roles, source paths and each counter timestamp. Refresh next turn to include the coordinator final response.')
foreach ($warning in $warnings) { $markdown += "- $warning" }
$markdown | Set-Content -LiteralPath (Join-Path $OutputDirectory 'TOKEN-USAGE.md') -Encoding UTF8
$summaries | Format-Table model,reasoning_effort,sessions,uncached_input_tokens,cached_input_tokens,output_tokens,total_tokens -AutoSize
if ($warnings.Count) { $warnings | Write-Warning }
