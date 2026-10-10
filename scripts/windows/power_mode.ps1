param(
    [ValidateSet('Read', 'Set')]
    [string]$Action = 'Read',
    [ValidateSet('Direct', 'Light', 'Battery')]
    [string]$Mode = 'Direct',
    [ValidateRange(1, 3600)]
    [int]$TelemetryIntervalSeconds,
    [string]$DeviceId = 'farm-01'
)

$ErrorActionPreference = 'Stop'
$saemsBase = 'http://192.168.4.1'
$saemsApAddress = Get-NetIPAddress -AddressFamily IPv4 -ErrorAction SilentlyContinue |
    Where-Object { $_.IPAddress -like '192.168.4.*' } |
    Select-Object -First 1

if (-not $saemsApAddress) {
    throw 'Laptop is not connected to the ESP32 AP. Join SAEMS-farm-01 first, then retry.'
}

try {
    $saemsConfig = Invoke-RestMethod "$saemsBase/v1/config" -TimeoutSec 10
} catch {
    throw "Cannot read ESP32 config at $saemsBase. In Battery mode, retry during the awake window. Details: $($_.Exception.Message)"
}

if ($saemsConfig.device_id -ne $DeviceId) {
    throw "Unexpected device '$($saemsConfig.device_id)'; expected '$DeviceId'."
}
if ($saemsConfig.PSObject.Properties.Name -notcontains 'power_mode') {
    throw 'Firmware does not expose runtime power_mode. Flash the current unified firmware once.'
}

if ($Action -eq 'Set') {
    $saemsRequestedMode = $Mode.ToLowerInvariant()
    $saemsBody = @{ power_mode = $saemsRequestedMode }
    if ($PSBoundParameters.ContainsKey('TelemetryIntervalSeconds')) {
        $saemsBody.telemetry_interval_s = $TelemetryIntervalSeconds
    }
    if ($saemsRequestedMode -eq 'battery' -and
        -not $PSBoundParameters.ContainsKey('TelemetryIntervalSeconds') -and
        [int]$saemsConfig.telemetry_interval_s -lt 30) {
        Write-Warning 'Battery mode works with the current interval, but 60 seconds or more is recommended for useful Deep Sleep time.'
    }
    $saemsJson = $saemsBody | ConvertTo-Json -Compress
    Invoke-RestMethod "$saemsBase/v1/config" -Method Put `
        -ContentType 'application/json' -Body $saemsJson -TimeoutSec 10 | Out-Null
    $saemsConfig = Invoke-RestMethod "$saemsBase/v1/config" -TimeoutSec 10
    if ($saemsConfig.power_mode -ne $saemsRequestedMode) {
        throw "Power mode readback is '$($saemsConfig.power_mode)', not '$saemsRequestedMode'."
    }
    Write-Output "Power mode '$saemsRequestedMode' persisted. It applies at the next sample boundary; no rebuild or reflash is needed."
}

$saemsConfig | Select-Object device_id,power_mode,telemetry_interval_s,power_mode_applies_next_cycle,ap_enabled,ble_enabled
