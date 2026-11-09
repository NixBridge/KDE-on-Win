param([string]$CraftRoot = 'D:\KDE\CraftRoot')
$ErrorActionPreference = 'Stop'
foreach ($module in 'org\kde\kirigami', 'org\kde\plasma\core', 'org\kde\plasma\activityswitcher', 'org\kde\kwindowsystem', 'org\kde\quickcharts', 'org\kde\quickcharts\controls') {
    if (-not (Test-Path -LiteralPath (Join-Path $CraftRoot "qml\$module\qmldir"))) {
        throw "Runtime incomplete: missing QML module $module"
    }
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'native\qt.conf') -Destination (Join-Path $CraftRoot 'bin\qt.conf')
foreach ($relative in 'plugins\kf6\kio\desktop.dll', 'plugins\kf6\kded\desktopnotifier.dll') {
    if (-not (Test-Path -LiteralPath (Join-Path $CraftRoot $relative))) { throw "Runtime incomplete: missing $relative" }
}
# Breeze disables its wallpaper install on Windows, but Plasma needs the package
# to finish loading the desktop and create the first panel.
$wallpaperRoot = Join-Path $CraftRoot 'bin\data\wallpapers'
$wallpaperSource = Join-Path $CraftRoot 'build\kde\plasma\breeze\work\breeze-6.7.4\wallpapers\Next'
if (-not (Test-Path -LiteralPath (Join-Path $wallpaperRoot 'Next\metadata.json'))) {
    if (-not (Test-Path -LiteralPath (Join-Path $wallpaperSource 'metadata.json'))) {
        throw 'Missing Breeze wallpaper source: build breeze from source before packaging.'
    }
    New-Item -ItemType Directory -Path $wallpaperRoot -Force | Out-Null
    Copy-Item -LiteralPath $wallpaperSource -Destination $wallpaperRoot -Recurse -Force
}
foreach ($relative in 'Next\metadata.json', 'Next\contents\images\5120x2880.png') {
    if (-not (Test-Path -LiteralPath (Join-Path $wallpaperRoot $relative))) {
        throw "Runtime incomplete: missing wallpaper $relative"
    }
}
& python (Join-Path $PSScriptRoot 'tools\verify-runtime-imports.py') --root $CraftRoot
if ($LASTEXITCODE) { throw 'Runtime import validation failed' }
$required = 'plasmashell.exe', 'dbus-daemon.exe', 'kded6.exe', 'kactivitymanagerd.exe', 'kbuildsycoca6.exe', 'konsole.exe', 'plasma-process-monitor.exe', 'plasma-kde-launcher.exe', 'plasma-text-editor.exe', 'plasma-system-settings.exe', 'plasma-window-decorations.exe', 'plasma-notification-bridge.exe', 'plasma-archive-manager.exe'
foreach ($style in 'plasmawindowsstyle.dll', 'breeze6.dll') {
    if (-not (Test-Path -LiteralPath (Join-Path $CraftRoot "plugins\styles\$style"))) { throw "Missing desktop style: $style" }
}
foreach ($name in $required) {
    if (-not (Test-Path -LiteralPath (Join-Path $CraftRoot "bin\$name"))) {
        throw "Runtime incomplete: missing $name"
    }
}
foreach ($relative in 'bin\Qt6Core.dll', 'bin\Qt6DBus.dll', 'plugins\platforms\qwindows.dll', 'plugins\sqldrivers\qsqlite.dll', 'plugins\kf6\org.kde.kwindowsystem.platforms\KF6WindowSystemWindowsPlugin.dll', 'qml\org\kde\plasma\windowsmenu\startmenuplugin.dll', 'plugins\plasma\applets\org.kde.plasma.windowsmenu.dll', 'plugins\plasma\applets\org.kde.plasma.volumewin.dll', 'plugins\plasma\applets\org.kde.plasma.imewin.dll', 'plugins\kactivitymanagerd1\org.kde.ActivityManager.ResourceScoring.dll') {
    if (-not (Test-Path -LiteralPath (Join-Path $CraftRoot $relative))) {
        throw "Runtime incomplete: missing $relative"
    }
}
foreach ($applet in 'windowsmenu', 'volumewin', 'imewin') {
    $metadata = Join-Path $CraftRoot "bin\data\plasma\plasmoids\org.kde.plasma.$applet\metadata.json"
    if (-not (Test-Path -LiteralPath $metadata)) { throw "Runtime incomplete: missing $metadata" }
}
$out = Join-Path $PSScriptRoot ('dist\' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
$stage = Join-Path $out 'PlasmaWindows'
New-Item -ItemType Directory -Path $stage -Force | Out-Null
foreach ($name in 'bin', 'lib', 'plugins', 'qml', 'share', 'etc', 'translations') {
    $source = Join-Path $CraftRoot $name
    if (Test-Path -LiteralPath $source) {
        & robocopy $source (Join-Path $stage $name) /E /R:2 /W:1 /MT:8 /XD .git blueprints /XF *.pdb *.lib *.a *.exp *.obj /NFL /NDL /NJH /NJS /NP | Out-Null
        if ($LASTEXITCODE -ge 8) { throw "Failed to stage $source" }
    }
}
$tools = Join-Path $PSScriptRoot 'upstream\plasma-windows\tools'
foreach ($name in 'deploy-vm.cmd', 'session-shell.cmd', 'plasma-common.cmd', 'plasma-shell.cmd', 'switch-shell.cmd', 'shell-registry.ps1', 'build-startmenu-desktops.ps1', 'vm-diagnostics.cmd') {
    Copy-Item -LiteralPath (Join-Path $tools $name) -Destination $stage
}
foreach ($name in 'deploy-vm.cmd', 'session-shell.cmd') {
    $path = Join-Path $stage $name
    $text = [IO.File]::ReadAllText($path)
    $text = $text.Replace('D:\Projects\CraftRoot', 'D:\KDE\CraftRoot').Replace('D:/Projects/CraftRoot', 'D:/KDE/CraftRoot').Replace('Projects\CraftRoot', 'KDE\CraftRoot')
    $prefixSetup = @'
rem Resolve the build prefix without expanding variables set inside a block.
if not exist "D:\KDE\CraftRoot\bin\plasmashell.exe" (
    for %%P in ("%CRAFT_ROOT%\..\..") do (
        if exist "%%~fP\KDE\CraftRoot\bin\plasmashell.exe" subst D: "%%~fP" >nul 2>&1
    )
)
if not exist "D:\KDE\CraftRoot\bin\plasmashell.exe" (
    echo ERROR: extract to D:\KDE\CraftRoot or C:\KDE\CraftRoot with D: available.
    exit /b 1
)

'@
    $pattern = '(?s)rem Make the craft-compiled.*?(?=if not exist "%CRAFT_BIN%\\plasmashell.exe")'
    if ([regex]::Matches($text, $pattern).Count -ne 1) { throw "Unexpected prefix setup in $name" }
    $text = [regex]::Replace($text, $pattern, [System.Text.RegularExpressions.MatchEvaluator]{ param($match) $prefixSetup })
    $text = $text.Replace("`r`n", "`n").Replace("`n", "`r`n")
    [IO.File]::WriteAllText($path, $text, [Text.UTF8Encoding]::new($false))
}
[IO.File]::WriteAllText((Join-Path $stage 'VM-TEST.txt'), "Extract this archive to D:\KDE\CraftRoot in the test VM, or C:\KDE\CraftRoot with drive D: available for a temporary SUBST mapping. Run deploy-vm.cmd with no arguments for a manual session. Optionally run run-vm-probe.cmd to check the window backend; results are written to bin\kws-probe-result.txt. Save logs before changing the login shell. This build has not passed VM acceptance yet.", [Text.UTF8Encoding]::new($false))
Copy-Item -LiteralPath (Join-Path $tools 'dbus\session-plasma.conf') -Destination (Join-Path $stage 'dbus-session-plasma.conf')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'manifests\source-verification.json') -Destination $stage
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'manifests\extra-source-verification.json') -Destination $stage
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'docs\PORTING.md') -Destination $stage
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'AI_NOTICE.md') -Destination $stage
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'THIRD_PARTY.md') -Destination $stage
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'LICENSE') -Destination (Join-Path $stage 'LICENSE.project')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'manifests\build-lock.json') -Destination $stage
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'manifests\runtime-import-verification.json') -Destination $stage
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'runtime\run-vm-probe.cmd') -Destination $stage
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'runtime\run-qml-probe.cmd') -Destination $stage
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'runtime\run-runtime-checks.cmd') -Destination $stage
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'runtime\apply-panel-opacity.cmd') -Destination $stage
foreach ($name in 'repair-taskbar-pins.cmd', 'repair-taskbar-pins.ps1', 'taskbar-pins-migration.js') {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot "runtime\$name") -Destination $stage
}
foreach ($name in 'start-konsole.cmd', 'start-process-monitor.cmd', 'start-dolphin.cmd', 'start-text-editor.cmd', 'start-system-settings.cmd', 'start-window-decorations.cmd', 'start-window-decorations-admin.cmd', 'start-window-decorations-admin.ps1', 'configure-kde-link-chooser.cmd', 'apply-client-style.cmd', 'install-kde-logon.cmd', 'restore-kde-logon.cmd', 'install-kde-logon.ps1', 'logon-deployment.ps1') {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot "runtime\$name") -Destination $stage
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'third_party') -Destination $stage -Recurse
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'docs') -Destination $stage -Recurse
foreach ($name in 'install-file-associations.cmd', 'install-file-associations.ps1', 'install-notifications.cmd', 'install-notifications.ps1', 'start-notifications.ps1', 'notifications-settings.cmd') {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot "runtime\$name") -Destination $stage
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'native\notifications\package') -Destination (Join-Path $stage 'notifications-package') -Recurse
foreach ($script in Get-ChildItem -LiteralPath $stage -File | Where-Object { $_.Extension -in '.cmd', '.ps1' }) {
    $text = [IO.File]::ReadAllText($script.FullName).Replace("`r`n", "`n").Replace("`n", "`r`n")
    [IO.File]::WriteAllText($script.FullName, $text, [Text.UTF8Encoding]::new($false))
}
$zip = Join-Path $out 'plasma-windows-vm.zip'
$sevenZip = Join-Path $CraftRoot 'dev-utils\7z\7za.exe'
& $sevenZip a -tzip -mx=3 $zip "$stage\*"
if ($LASTEXITCODE -ne 0) { throw 'Archive creation failed' }
$hash = Get-FileHash -LiteralPath $zip -Algorithm SHA256
[IO.File]::WriteAllText(($zip + '.sha256'), ($hash.Hash.ToLowerInvariant() + ' *' + (Split-Path $zip -Leaf) + "`n"), [Text.UTF8Encoding]::new($false))
$hash | Format-List
Write-Output $zip
