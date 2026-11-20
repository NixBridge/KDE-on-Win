# SPDX-License-Identifier: GPL-3.0-or-later
param(
    [Parameter(Mandatory=$true)][ValidateRange(1,10000)][int]$TimeoutMs,
    [string]$Name
)

$client = Join-Path $env:CRAFT_BIN 'dbus-send.exe'
if (-not [IO.File]::Exists($client)) { exit 1 }
$argsForBus = @('--session', '--print-reply', '--reply-timeout=250',
    '--dest=org.freedesktop.DBus', '/org/freedesktop/DBus')
if ($Name) {
    $argsForBus += @('org.freedesktop.DBus.GetNameOwner', "string:$Name")
} else {
    $argsForBus += 'org.freedesktop.DBus.GetId'
}
$clock = [Diagnostics.Stopwatch]::StartNew()
do {
    & $client @argsForBus 2>&1 | Out-Null
    if ($LASTEXITCODE -eq 0) { exit 0 }
    $remaining = $TimeoutMs - $clock.ElapsedMilliseconds
    if ($remaining -gt 0) { Start-Sleep -Milliseconds ([Math]::Min(75, $remaining)) }
} while ($clock.ElapsedMilliseconds -lt $TimeoutMs)
exit 1
