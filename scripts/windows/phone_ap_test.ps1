param([ValidateSet('Connect','Restore')][string]$Action = 'Connect')
$ErrorActionPreference = 'Stop'
$saemsAdb = 'C:\Users\Tien An\AppData\Local\Android\Sdk\platform-tools\adb.exe'
$saemsPhone = 'R58W80K3NWK'
$saemsRepo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$saemsOld = @{}
foreach ($saemsLine in (Get-Content -LiteralPath (Join-Path $saemsRepo '..\sdkconfig'))) {
    if ($saemsLine -match '^(CONFIG_SMARTFARM_WIFI_(SSID|PASSWORD))=(.*)$') {
        $saemsOld[$Matches[1]] = ConvertFrom-Json $Matches[3]
    }
}
$saemsStatePath = Join-Path $saemsRepo '.local\phone-network-before-ap.json'
if ($Action -eq 'Connect') {
    $saemsStatus = (& $saemsAdb -s $saemsPhone shell cmd wifi status) -join "`n"
    $saemsExpected = [regex]::Escape($saemsOld['CONFIG_SMARTFARM_WIFI_SSID'])
    if ($saemsStatus -notmatch ('Wifi is connected to "' + $saemsExpected + '"')) {
        throw 'Phone is not on the known original Wi-Fi; restore plan requires user input'
    }
    # This is generated runtime state, not a checked-in configuration file.
    [PSCustomObject]@{
        wifi = (& $saemsAdb -s $saemsPhone shell settings get global wifi_on).Trim()
        mobileData = (& $saemsAdb -s $saemsPhone shell settings get global mobile_data).Trim()
    } | ConvertTo-Json | Set-Content -Encoding UTF8 -LiteralPath $saemsStatePath
    $saemsAp = Get-Content (Join-Path $saemsRepo '.local\firmware-ap-access.json') -Raw | ConvertFrom-Json
    & $saemsAdb -s $saemsPhone shell svc data disable | Out-Null
    & $saemsAdb -s $saemsPhone shell cmd wifi connect-network $saemsAp.ssid wpa2 $saemsAp.password | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'AP connect request failed; run Restore' }
    Write-Output 'Requested password-protected ESP32 AP connection; cellular data temporarily disabled'
} else {
    $saemsState = Get-Content $saemsStatePath -Raw | ConvertFrom-Json
    & $saemsAdb -s $saemsPhone shell cmd wifi connect-network $saemsOld['CONFIG_SMARTFARM_WIFI_SSID'] wpa2 $saemsOld['CONFIG_SMARTFARM_WIFI_PASSWORD'] | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Original Wi-Fi restore request failed' }
    if ($saemsState.mobileData -eq '1') { & $saemsAdb -s $saemsPhone shell svc data enable | Out-Null }
    if ($saemsState.wifi -eq '0') { & $saemsAdb -s $saemsPhone shell svc wifi disable | Out-Null }
    Write-Output 'Requested original Wi-Fi and cellular-data restoration; credentials omitted'
}
