# SPDX-License-Identifier: GPL-3.0-or-later
param([switch]$ShowSettings)
$ErrorActionPreference = 'Stop'
$package = Get-AppxPackage -Name 'NixBridge.KDEonWinNotifications' | Select-Object -First 1
if ($package) {
    if (-not (Get-Process -Name 'plasma-notification-bridge' -ErrorAction SilentlyContinue)) {
        Start-Process ('shell:AppsFolder\' + $package.PackageFamilyName + '!Bridge')
    } elseif ($ShowSettings) {
        $env:DBUS_SESSION_BUS_ADDRESS = 'tcp:host=127.0.0.1,port=12443'
        & (Join-Path $PSScriptRoot 'bin\qdbus6.exe') org.nixbridge.WindowsNotifications /Bridge showSettings
    }
}
