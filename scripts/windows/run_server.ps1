<#
.SYNOPSIS
    Check SAEMS Internet server or follow Pi service logs.

.EXAMPLE
    .\scripts\windows\run_server.ps1
    .\scripts\windows\run_server.ps1 -Action Watch
    .\scripts\windows\run_server.ps1 -Action Logs
    .\scripts\windows\run_server.ps1 -Action Db
#>
param(
    [ValidateSet('Check', 'Latest', 'Watch', 'Logs', 'Db', 'Services')]
    [string]$Action = 'Check',
    [string]$ApiBase = 'https://admin.tailb1f0ba.ts.net',
    [string]$PiHost = '192.168.1.4',
    [string]$PiUser = 'admin',
    [ValidateRange(1, 60)]
    [int]$IntervalSeconds = 2
)

$ErrorActionPreference = 'Stop'
$saemsRepo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$saemsAccessPath = Join-Path $saemsRepo '.local\device-access.json'

function Get-SaemsHeaders {
    if (-not (Test-Path -LiteralPath $saemsAccessPath)) {
        throw "Missing $saemsAccessPath; do not put a token in this script."
    }
    $saemsAccess = Get-Content -LiteralPath $saemsAccessPath -Raw | ConvertFrom-Json
    if ([string]::IsNullOrWhiteSpace($saemsAccess.api_access_token)) {
        throw 'device-access.json has no API token.'
    }
    return @{ Authorization = "Bearer $($saemsAccess.api_access_token)" }
}

function Get-SaemsLatest {
    $saemsHeaders = Get-SaemsHeaders
    return Invoke-RestMethod "$ApiBase/api/v1/devices/farm-01/telemetry/latest" -Headers $saemsHeaders -TimeoutSec 15
}

function Show-SaemsLatest {
    $saemsLatest = Get-SaemsLatest
    $saemsLatest | Select-Object device_id,boot_id,seq,received_at,data_mode,
        temperature_c,humidity_pct,soil_raw,soil_digital,soil_moisture_pct,light_lux,pump_state,sensor_status |
        Format-List
}

switch ($Action) {
    'Check' {
        $saemsHealth = Invoke-RestMethod "$ApiBase/health" -TimeoutSec 15
        Write-Host "SERVER: $($saemsHealth.status)"
        Show-SaemsLatest
    }
    'Latest' { Show-SaemsLatest }
    'Watch' {
        Write-Host "Watching HTTPS API backed by SQLite every $IntervalSeconds second(s). Stop with Ctrl+C."
        $saemsPrevious = ''
        while ($true) {
            try {
                $saemsLatest = Get-SaemsLatest
                $saemsKey = "$($saemsLatest.boot_id)/$($saemsLatest.seq)"
                if ($saemsKey -ne $saemsPrevious) {
                    $saemsSoil = if ($null -eq $saemsLatest.soil_moisture_pct) { '--' } else { $saemsLatest.soil_moisture_pct }
                    Write-Host ("{0} | API<-SQLite | boot={1} seq={2} | T={3}C H={4}% soilRaw={5} soilPct={6} lux={7} pump={8}" -f `
                        $saemsLatest.received_at, $saemsLatest.boot_id, $saemsLatest.seq,
                        $saemsLatest.temperature_c, $saemsLatest.humidity_pct,
                        $saemsLatest.soil_raw, $saemsSoil, $saemsLatest.light_lux,
                        $saemsLatest.pump_state)
                    $saemsPrevious = $saemsKey
                }
            } catch {
                Write-Warning "API watch failed: $($_.Exception.Message)"
            }
            Start-Sleep -Seconds $IntervalSeconds
        }
    }
    'Logs' {
        Write-Host "Following MQTT, ingestor and API logs at $PiUser@$PiHost. Stop with Ctrl+C."
        Write-Host 'If SSH asks for the Pi password, type it and press Enter; password characters are hidden.'
        & ssh -o ConnectTimeout=10 "$PiUser@$PiHost" 'journalctl -u saems-mqtt -u saems-ingestor -u saems-legacy-ingestor -u saems-api -f -n 40 -o short-iso'
        if ($LASTEXITCODE -ne 0) { throw "SSH log failed for $PiUser@$PiHost. Use LAN 192.168.1.4, or update the Tailscale SSH ACL before using 100.104.200.110." }
    }
    'Db' {
        Write-Host "Watching SQLite rows at $PiUser@$PiHost. Stop with Ctrl+C."
        $saemsDbCommand = "while true; do date -Is; sqlite3 /home/admin/saems-mini-server/data/telemetry.db 'SELECT device_id,boot_id,seq,received_at FROM telemetry ORDER BY id DESC LIMIT 5;'; sleep $IntervalSeconds; done"
        Write-Host 'If SSH asks for the Pi password, type it and press Enter; password characters are hidden.'
        & ssh -o ConnectTimeout=10 "$PiUser@$PiHost" $saemsDbCommand
        if ($LASTEXITCODE -ne 0) { throw "SQLite watch failed for $PiUser@$PiHost." }
    }
    'Services' {
        Write-Host 'If SSH asks for the Pi password, type it and press Enter; password characters are hidden.'
        & ssh -o ConnectTimeout=10 "$PiUser@$PiHost" 'systemctl is-active saems-mqtt saems-ingestor saems-legacy-ingestor saems-api tailscaled'
        if ($LASTEXITCODE -ne 0) { throw "Service status check failed for $PiUser@$PiHost." }
    }
}
