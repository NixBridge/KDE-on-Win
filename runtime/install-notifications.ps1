# SPDX-License-Identifier: GPL-3.0-or-later
param([switch]$Uninstall)
$ErrorActionPreference = 'Stop'
$name = 'NixBridge.KDEonWinNotifications'
if ($Uninstall) {
    Get-AppxPackage -Name $name | Remove-AppxPackage
    Write-Host 'Notification package removed for the current user.'
    exit
}
$manifest = Join-Path $PSScriptRoot 'notifications-package\AppxManifest.xml'
if (-not (Test-Path -LiteralPath $manifest)) { throw 'Notification package files are missing. Extract the complete cumulative overlay first.' }
$developer = Get-ItemPropertyValue -Path 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\AppModelUnlock' -Name AllowDevelopmentWithoutDevLicense -ErrorAction SilentlyContinue
if ($developer -ne 1) {
    Write-Host 'Windows requires Developer Mode to register this unsigned development package.'
    Write-Host 'Enable Developer Mode in the test VM, then run this script again.'
    Start-Process 'ms-settings:developers'
    exit 2
}
# Register a loose development package in CraftRoot; no certificate or trust-store changes.
# The executable lives one directory above the package manifest, so stage a manifest at root.
$rootManifest = Join-Path $PSScriptRoot 'AppxManifest.xml'
$xml = [IO.File]::ReadAllText($manifest).Replace('Assets\', 'notifications-package\Assets\')
[IO.File]::WriteAllText($rootManifest, $xml, [Text.UTF8Encoding]::new($false))
Add-AppxPackage -Register $rootManifest -ForceApplicationShutdown
& (Join-Path $PSScriptRoot 'start-notifications.ps1') -ShowSettings
