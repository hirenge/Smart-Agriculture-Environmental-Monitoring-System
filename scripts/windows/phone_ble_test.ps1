param(
    [ValidateSet('Isolate', 'Restore')]
    [string]$Action = 'Isolate',
    [string]$Serial
)

$ErrorActionPreference = 'Stop'
$saemsAdb = 'C:\Users\Tien An\AppData\Local\Android\Sdk\platform-tools\adb.exe'
$saemsRepo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$saemsStatePath = Join-Path $saemsRepo '.local\phone-network-before-ble.json'

function Resolve-SaemsPhone {
    if ($Serial) { return $Serial }
    $saemsDevice = & $saemsAdb devices | Select-Object -Skip 1 |
        Where-Object { $_ -match "\tdevice\s*$" -and $_ -notmatch '^emulator-' } |
        Select-Object -First 1
    if (-not $saemsDevice) { throw 'Physical Android phone was not found through ADB.' }
    return ($saemsDevice -split '\s+')[0]
}

if (-not (Test-Path -LiteralPath $saemsAdb)) { throw 'ADB was not found.' }
$saemsPhone = Resolve-SaemsPhone

if ($Action -eq 'Isolate') {
    [PSCustomObject]@{
        serial = $saemsPhone
        wifi = (& $saemsAdb -s $saemsPhone shell settings get global wifi_on).Trim()
        mobileData = (& $saemsAdb -s $saemsPhone shell settings get global mobile_data).Trim()
    } | ConvertTo-Json | Set-Content -Encoding UTF8 -LiteralPath $saemsStatePath
    & $saemsAdb -s $saemsPhone shell svc wifi disable | Out-Null
    & $saemsAdb -s $saemsPhone shell svc data disable | Out-Null
    Write-Host 'BLE TEST READY: Wi-Fi OFF, mobile data OFF. Keep Bluetooth ON on the phone.'
} else {
    if (-not (Test-Path -LiteralPath $saemsStatePath)) { throw 'No saved phone network state to restore.' }
    $saemsState = Get-Content -LiteralPath $saemsStatePath -Raw | ConvertFrom-Json
    if ($saemsState.wifi -eq '1') {
        & $saemsAdb -s $saemsPhone shell svc wifi enable | Out-Null
    } else {
        & $saemsAdb -s $saemsPhone shell svc wifi disable | Out-Null
    }
    if ($saemsState.mobileData -eq '1') {
        & $saemsAdb -s $saemsPhone shell svc data enable | Out-Null
    } else {
        & $saemsAdb -s $saemsPhone shell svc data disable | Out-Null
    }
    Write-Host 'PHONE NETWORK RESTORED.'
}
