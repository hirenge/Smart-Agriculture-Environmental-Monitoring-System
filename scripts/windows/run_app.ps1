<#
.SYNOPSIS
    Build or run the SAEMS Flutter app with real Internet server data.

.EXAMPLE
    .\scripts\windows\run_app.ps1 -Action Run -Target A24
    .\scripts\windows\run_app.ps1 -Action Run -Target Pixel6
    .\scripts\windows\run_app.ps1 -Action Build
#>
param(
    [ValidateSet('Build', 'Install', 'Run')]
    [string]$Action = 'Run',
    [ValidateSet('A24', 'Pixel6')]
    [string]$Target = 'A24',
    [string]$Serial,
    [switch]$Mock
)

$ErrorActionPreference = 'Stop'
$saemsRepo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$saemsApp = Join-Path $saemsRepo 'mobile\saems_app'
$saemsFlutter = 'C:\puro-smartfarm\envs\smartfarm\flutter\bin\flutter.bat'
$saemsAdb = 'C:\Users\Tien An\AppData\Local\Android\Sdk\platform-tools\adb.exe'
$env:PUB_CACHE = 'C:\puro-smartfarm\shared\pub_cache'
$env:JAVA_HOME = 'C:\Program Files\Android\Android Studio\jbr'
$env:ANDROID_HOME = 'C:\Users\Tien An\AppData\Local\Android\Sdk'

if (-not (Test-Path -LiteralPath $saemsFlutter)) { throw 'Flutter was not found.' }
if (-not (Test-Path -LiteralPath $saemsAdb)) { throw 'Android SDK/ADB was not found.' }

function Resolve-SaemsSerial {
    if ($Serial) { return $Serial }
    $saemsDevices = @(& $saemsAdb devices | Select-Object -Skip 1 | Where-Object { $_ -match "\tdevice\s*$" })
    if ($Target -eq 'Pixel6') {
        $saemsMatch = $saemsDevices | Where-Object { $_ -match '^emulator-' } | Select-Object -First 1
    } else {
        $saemsMatch = $saemsDevices | Where-Object { $_ -notmatch '^emulator-' } | Select-Object -First 1
    }
    if (-not $saemsMatch) { throw "Target $Target was not found. Run adb devices and check USB debugging/emulator." }
    return ($saemsMatch -split '\s+')[0]
}

function Invoke-SaemsFlutter([string[]]$saemsArguments) {
    Push-Location $saemsApp
    try {
        & $saemsFlutter @saemsArguments
        if ($LASTEXITCODE -ne 0) { throw "Flutter failed with exit code $LASTEXITCODE" }
    } finally { Pop-Location }
}

$saemsDefines = @(
    '--dart-define=ENABLE_MOCK_MODE=' + ($(if ($Mock) { 'true' } else { 'false' })),
    '--dart-define=API_BASE_URL=https://admin.tailb1f0ba.ts.net',
    '--dart-define=DEFAULT_DEVICE_ID=farm-01'
)

Push-Location $saemsApp
try {
    & $saemsFlutter pub get
    if ($LASTEXITCODE -ne 0) { throw 'flutter pub get failed.' }
} finally { Pop-Location }

if ($Action -eq 'Build') {
    Invoke-SaemsFlutter (@('build', 'apk', '--debug') + $saemsDefines)
    Write-Host 'APP BUILD OK: mobile/saems_app/build/app/outputs/flutter-apk/app-debug.apk'
    exit 0
}

$saemsSerial = Resolve-SaemsSerial
if ($Action -eq 'Install') {
    Invoke-SaemsFlutter (@('build', 'apk', '--debug') + $saemsDefines)
    & $saemsAdb -s $saemsSerial install -r (Join-Path $saemsApp 'build\app\outputs\flutter-apk\app-debug.apk')
    if ($LASTEXITCODE -ne 0) { throw 'APK install failed.' }
    Write-Host "APP INSTALL OK: $saemsSerial"
    exit 0
}

Write-Host "Running real app on $Target ($saemsSerial)."
Write-Host 'Use the emulator/phone screen for app actions; keep this terminal open for Flutter logs.'
Write-Host 'Press q in this terminal only when you want to stop the app.'
Invoke-SaemsFlutter (@('run', '--no-pub', '-d', $saemsSerial) + $saemsDefines)
