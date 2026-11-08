# SPDX-License-Identifier: GPL-3.0-or-later
param([string]$CraftRoot = $PSScriptRoot, [int]$WaitSeconds = 45)
$ErrorActionPreference = 'Stop'
$qdbus = Join-Path $CraftRoot 'bin\qdbus.exe'
$migration = Join-Path $PSScriptRoot 'taskbar-pins-migration.js'
if (-not (Test-Path -LiteralPath $qdbus)) { throw 'qdbus.exe not found in CraftRoot' }
$script = [IO.File]::ReadAllText($migration)
$toolsAvailable = (Test-Path -LiteralPath (Join-Path $CraftRoot 'bin\konsole.exe')) -and (Test-Path -LiteralPath (Join-Path $CraftRoot 'bin\plasma-process-monitor.exe')) -and (Test-Path -LiteralPath (Join-Path $CraftRoot 'plugins\styles\plasmawindowsstyle.dll'))
$script = 'var kdeDesktopToolsAvailable = ' + $toolsAvailable.ToString().ToLowerInvariant() + ";`n" + $script
$env:DBUS_SESSION_BUS_ADDRESS = 'tcp:host=127.0.0.1,port=12443'
$timer = [Diagnostics.Stopwatch]::StartNew()
do {
    try {
        $result = & $qdbus org.kde.plasmashell /PlasmaShell org.kde.PlasmaShell.evaluateScript $script 2>&1
        if ($LASTEXITCODE -eq 0 -and (($result -join "`n") -match 'TASKBAR_PINS_REPAIRED=')) {
            $logDir = Join-Path $env:LOCALAPPDATA 'plasma-win-session'
            [IO.Directory]::CreateDirectory($logDir) | Out-Null
            [IO.File]::WriteAllText((Join-Path $logDir 'taskbar-pins-migration.log'), ($result -join "`r`n"), [Text.UTF8Encoding]::new($false))
            Write-Output 'Taskbar pins repaired; other pinned applications were preserved.'
            exit 0
        }
    } catch {
        # The session may not own its bus name yet during startup.
    }
    Start-Sleep -Milliseconds 500
} while ($timer.Elapsed.TotalSeconds -lt $WaitSeconds)
Write-Error 'Plasma session not available. Start deploy-vm.cmd and retry.'
exit 1
