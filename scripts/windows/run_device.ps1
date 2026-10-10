<#
.SYNOPSIS
    Build, flash, or monitor the SAEMS ESP32-S3 firmware.

.EXAMPLE
    .\scripts\windows\run_device.ps1 -Action Build -PowerProfile Direct
    .\scripts\windows\run_device.ps1 -Action Flash -PowerProfile Direct
    .\scripts\windows\run_device.ps1 -Action Read
    .\scripts\windows\run_device.ps1 -Action Monitor

.NOTES
    PowerProfile selects only the first-boot default when NVS has no power_mode.
    The built binary contains Direct, Light, and Battery; switch at runtime with
    the mobile app or scripts/windows/power_mode.ps1 instead of reflashing.
#>
param(
    [ValidateSet('Build', 'Flash', 'Read', 'Monitor')]
    [string]$Action = 'Build',
    [string]$Port = 'COM3',
    [switch]$NoSoilRaw,
    [switch]$FullBackup,
    [ValidateSet('Direct', 'Light', 'Battery')]
    [string]$PowerProfile = 'Direct',
    [ValidateRange(5, 300)]
    [int]$Seconds = 30,
    [ValidateRange(9600, 2000000)]
    [int]$BackupBaud = 115200,
    [ValidateRange(1, 5)]
    [int]$BackupAttempts = 3
)

$ErrorActionPreference = 'Stop'
$saemsRepo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$saemsFirmware = Join-Path $saemsRepo 'firmware\esp32s3'
$saemsPython = 'C:\Espressif\tools\python\v6.0.3\venv\Scripts\python.exe'
$saemsIdfProfile = 'C:\Espressif\tools\Microsoft.v6.0.3.PowerShell_profile.ps1'
$saemsIdfRunner = Join-Path $saemsRepo 'scripts\windows\idf_private.py'
$saemsPrepare = Join-Path $saemsRepo 'scripts\windows\prepare_firmware_acceptance.py'
$saemsReader = Join-Path $saemsRepo 'scripts\windows\read_device_log.py'
$saemsMigration = Join-Path $saemsRepo 'scripts\windows\check_flash_migration.py'
$saemsBuild = Join-Path $saemsFirmware 'build-internet'
$saemsLocal = Join-Path $saemsRepo '.local'

if (-not (Test-Path -LiteralPath $saemsIdfProfile)) {
    throw "ESP-IDF profile not found: $saemsIdfProfile"
}
. $saemsIdfProfile | Out-Null
$env:IDF_PYTHON_ENV_PATH = 'C:\Espressif\tools\python\v6.0.3\venv'

function Stop-Saems([string]$saemsMessage) {
    throw "[SAEMS device] $saemsMessage"
}

function Test-SaemsPort {
    if (-not ([System.IO.Ports.SerialPort]::GetPortNames() -contains $Port)) {
        Stop-Saems "$Port is not available. Reconnect USB data and close Serial Monitor."
    }
}

function Test-SaemsSerialOpen {
    Test-SaemsPort
    $saemsSerial = New-Object System.IO.Ports.SerialPort
    $saemsSerial.PortName = $Port
    $saemsSerial.BaudRate = 115200
    $saemsSerial.DataBits = 8
    $saemsSerial.Parity = [System.IO.Ports.Parity]::None
    $saemsSerial.StopBits = [System.IO.Ports.StopBits]::One
    $saemsSerial.DtrEnable = $false
    $saemsSerial.RtsEnable = $false
    try {
        $saemsSerial.Open()
    } catch {
        Stop-Saems "$Port is listed but cannot be opened. Close every Serial Monitor, unplug/replug the ESP32 USB cable, wait for $Port to return, then retry. Details: $($_.Exception.Message)"
    } finally {
        if ($saemsSerial.IsOpen) { $saemsSerial.Close() }
        $saemsSerial.Dispose()
    }
}

function Start-SaemsIdf([string[]]$saemsArguments) {
    Push-Location $saemsFirmware
    try {
        & $saemsPython $saemsIdfRunner @saemsArguments
        if ($LASTEXITCODE -ne 0) { Stop-Saems "ESP-IDF failed with exit code $LASTEXITCODE" }
    } finally {
        Pop-Location
    }
}

function Prepare-SaemsConfig {
    $saemsArguments = @($saemsPrepare, '--power-profile', $PowerProfile.ToLowerInvariant())
    if (-not $NoSoilRaw) { $saemsArguments += '--soil-raw' }
    & $saemsPython @saemsArguments
    if ($LASTEXITCODE -ne 0) { Stop-Saems 'Could not prepare private sdkconfig.' }
}

function Build-SaemsFirmware {
    Prepare-SaemsConfig
    $saemsIdfArgs = @('-B', 'build-internet', 'build')
    Start-SaemsIdf -saemsArguments $saemsIdfArgs
    $saemsBinary = Join-Path $saemsBuild 'saems_esp32s3.bin'
    if (-not (Test-Path -LiteralPath $saemsBinary)) { Stop-Saems 'saems_esp32s3.bin was not produced.' }
    $saemsHash = (Get-FileHash -LiteralPath $saemsBinary -Algorithm SHA256).Hash.ToLowerInvariant()
    Write-Host "BUILD OK: firmware/esp32s3/build-internet/saems_esp32s3.bin"
    Write-Host "SHA256: $saemsHash"
}

function Backup-SaemsFlash {
    New-Item -ItemType Directory -Path $saemsLocal -Force | Out-Null
    $saemsBackup = Join-Path $saemsLocal 'esp32-before-lm393-current.bin'
    if (Test-Path -LiteralPath $saemsBackup) {
        if ((Get-Item -LiteralPath $saemsBackup).Length -ne 0x1000000) {
            Stop-Saems "Backup exists but is not 16 MB: $saemsBackup"
        }
        Write-Host 'Using existing 16 MB flash backup; will not overwrite it.'
        return (Split-Path -Leaf $saemsBackup)
    }
    $saemsPartial = "$saemsBackup.partial"
    if (Test-Path -LiteralPath $saemsPartial) { Remove-Item -LiteralPath $saemsPartial -Force }
    Test-SaemsSerialOpen
    Write-Host "Backing up all 16 MB flash at $BackupBaud baud; this can take several minutes..."
    for ($saemsAttempt = 1; $saemsAttempt -le $BackupAttempts; $saemsAttempt++) {
        if (Test-Path -LiteralPath $saemsPartial) { Remove-Item -LiteralPath $saemsPartial -Force }
        Write-Host "Backup attempt $saemsAttempt/$BackupAttempts..."
        $saemsBackupOutput = & $saemsPython -m esptool --chip esp32s3 --port $Port --baud $BackupBaud `
            --before default-reset --after no-reset --connect-attempts 5 `
            read-flash 0 0x1000000 $saemsPartial 2>&1
        $saemsExitCode = $LASTEXITCODE
        $saemsBackupOutput | ForEach-Object { Write-Host $_ }
        $saemsBackupComplete = Test-Path -LiteralPath $saemsPartial -and
            (Get-Item -LiteralPath $saemsPartial).Length -eq 0x1000000
        if ($saemsExitCode -eq 0 -and $saemsBackupComplete) {
            Move-Item -LiteralPath $saemsPartial -Destination $saemsBackup
            Write-Host "BACKUP OK: $saemsBackup"
            return (Split-Path -Leaf $saemsBackup)
        }
        if (Test-Path -LiteralPath $saemsPartial) { Remove-Item -LiteralPath $saemsPartial -Force }
        if ($saemsAttempt -lt $BackupAttempts) {
            Write-Warning 'Backup transfer failed. Retrying after a short pause; no erase or flash has run.'
            Start-Sleep -Seconds 2
        }
    }
    Stop-Saems 'Flash backup failed after all retries; stopped before erase or flash. Reconnect USB and retry.'
}

function Get-SaemsMigrationProbe {
    New-Item -ItemType Directory -Path $saemsLocal -Force | Out-Null
    $saemsProbe = Join-Path $saemsLocal 'esp32-migration-probe.bin'
    if (Test-Path -LiteralPath $saemsProbe) { Remove-Item -LiteralPath $saemsProbe -Force }
    Write-Host 'Reading 28 KiB partition/NVS probe (normally a few seconds)...'
    $saemsProbeOutput = & $saemsPython -m esptool --chip esp32s3 --port $Port --baud 115200 `
        --before default-reset --after no-reset --connect-attempts 5 `
        read-flash 0x8000 0x7000 $saemsProbe 2>&1
    $saemsProbeExitCode = $LASTEXITCODE
    $saemsProbeOutput | ForEach-Object { Write-Host $_ }
    if ($saemsProbeExitCode -ne 0 -or -not (Test-Path -LiteralPath $saemsProbe) -or
        (Get-Item -LiteralPath $saemsProbe).Length -ne 0x7000) {
        if (Test-Path -LiteralPath $saemsProbe) { Remove-Item -LiteralPath $saemsProbe -Force }
        Stop-Saems 'Partition/NVS probe failed; stopped before erase or flash.'
    }
    return (Split-Path -Leaf $saemsProbe)
}

function Flash-SaemsFirmware {
    Test-SaemsSerialOpen
    Build-SaemsFirmware
    $saemsSnapshotName = 'esp32-lm393-current-nvs.bin'
    $saemsSnapshotPath = Join-Path $saemsLocal $saemsSnapshotName
    if (Test-Path -LiteralPath $saemsSnapshotPath) { Remove-Item -LiteralPath $saemsSnapshotPath -Force }
    if ($FullBackup) {
        $saemsInputName = Backup-SaemsFlash
        $saemsMigrationArgs = @($saemsMigration, '--backup', $saemsInputName,
            '--snapshot', $saemsSnapshotName)
    } else {
        $saemsInputName = Get-SaemsMigrationProbe
        $saemsMigrationArgs = @($saemsMigration, '--backup', $saemsInputName,
            '--snapshot', $saemsSnapshotName, '--base-address', '0x8000')
    }
    $saemsMigrationOutput = & $saemsPython @saemsMigrationArgs 2>&1
    $saemsMigrationOutput | ForEach-Object { Write-Host $_ }
    if ($LASTEXITCODE -ne 0) { Stop-Saems 'Partition layout is unexpected; stopped before flash.' }
    if ($saemsMigrationOutput -match 'MIGRATION_REQUIRED=1') {
        Write-Host 'Old partition: erasing only NVS expansion 0xF000..0x39000.'
        & $saemsPython -m esptool --chip esp32s3 --port $Port erase-region 0xF000 0x2A000
        if ($LASTEXITCODE -ne 0) { Stop-Saems 'NVS expansion erase failed; stopped before writing firmware.' }
    } else {
        Write-Host 'SAEMS 192 KiB NVS partition detected; no expansion erase needed.'
    }
    $saemsIdfArgs = @('-B', 'build-internet', '-p', $Port, 'flash')
    Start-SaemsIdf -saemsArguments $saemsIdfArgs
    Write-Host 'FLASH OK: SAEMS firmware written to ESP32-S3.'
    Write-Host "Next: .\scripts\windows\run_device.ps1 -Action Read -Port $Port"
}

if ($Action -eq 'Build') {
    Build-SaemsFirmware
} elseif ($Action -eq 'Flash') {
    Flash-SaemsFirmware
} elseif ($Action -eq 'Read') {
    Test-SaemsSerialOpen
    & $saemsPython $saemsReader --port $Port --seconds $Seconds --output 'lm393-demo.log'
    if ($LASTEXITCODE -ne 0) { Stop-Saems 'UART read failed.' }
} elseif ($Action -eq 'Monitor') {
    Test-SaemsSerialOpen
    $saemsMonitorArgs = @("-B", "build-internet", "-p", $Port, "monitor")
    Start-SaemsIdf -saemsArguments $saemsMonitorArgs
}
