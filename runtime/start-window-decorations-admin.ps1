# SPDX-License-Identifier: GPL-3.0-or-later
$ErrorActionPreference='Stop'
$program=Join-Path $PSScriptRoot 'bin\plasma-window-decorations.exe'
if(-not (Test-Path -LiteralPath $program -PathType Leaf)){throw "Missing decoration service: $program"}
Start-Process -FilePath $program -ArgumentList '--replace' -Verb RunAs -WindowStyle Hidden
