param(
    [ValidateSet('Dump','Tap','ApiToken','Screenshot','Interval')]
    [string]$Action = 'Dump',
    [string]$Serial = 'emulator-5554',
    [string]$Label,
    [string]$ScreenshotName = 'saems-debug.png',
    [ValidateRange(1,3600)][int]$Interval = 10
)
$ErrorActionPreference = 'Stop'
$saemsAdb = 'C:\Users\Tien An\AppData\Local\Android\Sdk\platform-tools\adb.exe'
$saemsRepo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
function Get-SaemsNodes {
    & $saemsAdb -s $Serial shell uiautomator dump /sdcard/saems-debug-ui.xml | Out-Null
    [xml]$saemsXml = (& $saemsAdb -s $Serial shell cat /sdcard/saems-debug-ui.xml) -join "`n"
    return $saemsXml.SelectNodes('//node')
}
function Invoke-SaemsTap($saemsNode) {
    $saemsBounds = [regex]::Matches($saemsNode.GetAttribute('bounds'),'\d+')
    if ($saemsBounds.Count -ne 4) { throw 'Invalid UI bounds' }
    $saemsX = [int](([int]$saemsBounds[0].Value + [int]$saemsBounds[2].Value) / 2)
    $saemsY = [int](([int]$saemsBounds[1].Value + [int]$saemsBounds[3].Value) / 2)
    & $saemsAdb -s $Serial shell input tap $saemsX $saemsY | Out-Null
}
switch ($Action) {
    'Interval' {
        $saemsNode = Get-SaemsNodes | Where-Object {
            $_.GetAttribute('class') -eq 'android.widget.EditText' -and
            $_.GetAttribute('text') -match '^\d{1,4}$' -and
            $_.GetAttribute('password') -ne 'true'
        } | Select-Object -Last 1
        if (!$saemsNode) { throw 'Open the provisioning interval field first' }
        $saemsLength = $saemsNode.GetAttribute('text').Length
        Invoke-SaemsTap $saemsNode
        Start-Sleep -Milliseconds 700
        & $saemsAdb -s $Serial shell input keyevent 123 | Out-Null
        for ($saemsIndex=0; $saemsIndex -lt $saemsLength; $saemsIndex++) {
            & $saemsAdb -s $Serial shell input keyevent 67 | Out-Null
        }
        & $saemsAdb -s $Serial shell input text "$Interval" | Out-Null
        Start-Sleep -Milliseconds 500
        $saemsImeState = (& $saemsAdb -s $Serial shell dumpsys input_method) -join "`n"
        if ($saemsImeState -match 'mInputShown=true|mIsInputViewShown=true') {
            & $saemsAdb -s $Serial shell input keyevent 4 | Out-Null
        }
        Write-Output "Entered telemetry interval: $Interval seconds"
    }
    'Dump' {
        foreach ($saemsNode in (Get-SaemsNodes)) {
            $saemsText = $saemsNode.GetAttribute('text')
            $saemsDescription = $saemsNode.GetAttribute('content-desc')
            if ($saemsNode.GetAttribute('password') -eq 'true') {
                $saemsText = '[secret field; contents omitted]'
            }
            if ($saemsText -or $saemsDescription) {
                [PSCustomObject]@{Text=$saemsText;Label=$saemsDescription;
                    Bounds=$saemsNode.GetAttribute('bounds');
                    Clickable=$saemsNode.GetAttribute('clickable')}
            }
        }
    }
    'Tap' {
        if (!$Label) { throw 'Specify a visible UI label' }
        $saemsMatches = @(Get-SaemsNodes | Where-Object {
            $_.GetAttribute('text').StartsWith($Label) -or
            $_.GetAttribute('content-desc').StartsWith($Label)
        })
        $saemsNode = $saemsMatches | Where-Object {
            $_.GetAttribute('clickable') -eq 'true'
        } | Select-Object -First 1
        if (!$saemsNode) { $saemsNode = $saemsMatches | Select-Object -First 1 }
        if (!$saemsNode) { throw "No visible UI label: $Label" }
        Invoke-SaemsTap $saemsNode
    }
    'ApiToken' {
        # Only enter the approved token through the visible password field.
        # Never embed a token in an APK, intent, screenshot output, or log.
        $saemsNode = Get-SaemsNodes | Where-Object {
            $_.GetAttribute('class') -eq 'android.widget.EditText' -and
            $_.GetAttribute('password') -eq 'true'
        } | Select-Object -First 1
        if (!$saemsNode) { throw 'Open the API token field in Settings first' }
        $saemsAccess = Get-Content (Join-Path $saemsRepo '.local\device-access.json') -Raw | ConvertFrom-Json
        if ($saemsAccess.api_access_token -notmatch '^[a-zA-Z0-9_-]{32,}$') {
            throw 'Unexpected token format; use manual secure entry'
        }
        Invoke-SaemsTap $saemsNode
        & $saemsAdb -s $Serial shell input text $saemsAccess.api_access_token | Out-Null
        & $saemsAdb -s $Serial shell input keyevent 4 | Out-Null
        Write-Output 'Token entered in the visible secret field; value omitted'
    }
    'Screenshot' {
        if ($ScreenshotName -notmatch '^[a-zA-Z0-9_-]+\.png$') { throw 'Invalid screenshot name' }
        & $saemsAdb -s $Serial shell screencap -p /sdcard/saems-debug-screen.png | Out-Null
        & $saemsAdb -s $Serial pull /sdcard/saems-debug-screen.png (Join-Path $saemsRepo ".local\$ScreenshotName")
    }
}
