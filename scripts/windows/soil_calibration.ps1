param(
    [ValidateSet('Read','Calibrate')][string]$Action = 'Read',
    [ValidateRange(0,4095)][int]$DryRaw,
    [ValidateRange(0,4095)][int]$WetRaw,
    [string]$DeviceId = 'farm-01'
)
$ErrorActionPreference = 'Stop'
# Join the ESP32 WPA2 AP on this laptop first; the public server cannot provision soil.
$saemsBase = 'http://192.168.4.1'
$saemsApAddress = Get-NetIPAddress -AddressFamily IPv4 -ErrorAction SilentlyContinue |
    Where-Object { $_.IPAddress -like '192.168.4.*' } |
    Select-Object -First 1
if (-not $saemsApAddress) {
    throw 'Laptop is not connected to the ESP32 AP. Join SAEMS-farm-01 first, then retry.'
}
$saemsConfig = Invoke-RestMethod "$saemsBase/v1/config" -TimeoutSec 10
if ($saemsConfig.device_id -ne $DeviceId) { throw 'Unexpected device; refusing configuration' }
if ($Action -eq 'Calibrate') {
    if (!$PSBoundParameters.ContainsKey('DryRaw') -or !$PSBoundParameters.ContainsKey('WetRaw')) {
        throw 'Provide BOTH measured -DryRaw and -WetRaw values'
    }
    if ([Math]::Abs($DryRaw - $WetRaw) -lt 100) {
        throw 'ADC span below 100 counts: check wiring and repeat the two measurements'
    }
    if ($saemsConfig.PSObject.Properties.Name -notcontains 'soil_calibrated') {
        throw 'Firmware lacks calibration guard; flash the current modular firmware first'
    }
    $saemsBody = @{
        soil_enabled = $true
        soil_calibrated = $true
        soil_dry_raw = $DryRaw
        soil_wet_raw = $WetRaw
    } | ConvertTo-Json -Compress
    Invoke-RestMethod "$saemsBase/v1/config" -Method Put -ContentType 'application/json' -Body $saemsBody -TimeoutSec 10 | Out-Null
    $saemsConfig = Invoke-RestMethod "$saemsBase/v1/config" -TimeoutSec 10
    if (!$saemsConfig.soil_calibrated -or !$saemsConfig.soil_enabled -or
        $saemsConfig.soil_dry_raw -ne $DryRaw -or $saemsConfig.soil_wet_raw -ne $WetRaw) {
        throw 'Calibration readback did not match; do not accept percentage output'
    }
    Write-Output 'Calibration persisted and verified; press ESP32 EN/reset to apply sensor settings (loads disconnected). No reflash needed.'
}
$saemsConfig | Select-Object device_id,soil_enabled,soil_calibrated,soil_raw_readout_enabled,soil_dry_raw,soil_wet_raw
Invoke-RestMethod "$saemsBase/v1/telemetry/latest" -TimeoutSec 10 |
    Select-Object boot_id,seq,soil_raw,soil_digital,soil_moisture_pct,sensor_status
