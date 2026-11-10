# SPDX-License-Identifier: GPL-3.0-or-later
"""Run the production Shell transaction with mocked registry cmdlets; no real registry writes."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='fix25-shell-',dir=root/'work') as directory:
    folder=Path(directory);driver=folder/'test.ps1'
    driver.write_text(r'''$ErrorActionPreference='Stop'
$script:current='custom-original-shell.exe'
$script:failNext=$false
function Test-Path {param([string]$LiteralPath) if($LiteralPath -eq $env:SHELL_KEY_PS){return $true};Microsoft.PowerShell.Management\Test-Path -LiteralPath $LiteralPath}
function Get-ItemProperty {param($LiteralPath) [pscustomobject]@{Shell=$script:current}}
function New-Item {param($Path,[switch]$Force) throw 'Existing registry keys must never be recreated'}
function New-ItemProperty {param($LiteralPath,$Name,$Value,$PropertyType,[switch]$Force) if($script:failNext){$script:failNext=$false;throw 'synthetic write failure'};$script:current=$Value}
function Check([bool]$ok){if(-not $ok){throw 'regression assertion failed'}}
$env:SHELL_KEY_PS='HKCU:\Software\NixBridge\SyntheticOnly'
$env:SHELL_BACKUP=Join-Path $env:TEST_DIRECTORY 'shell-backup.txt'
$env:SHELL_VALUE='new-shell.exe'
. $env:TEST_SOURCE -Action install
Check ($script:current -eq 'new-shell.exe')
Check ([IO.File]::ReadAllText($env:SHELL_BACKUP) -eq 'custom-original-shell.exe')
$env:SHELL_VALUE='moved-shell.exe'
. $env:TEST_SOURCE -Action install
Check ([IO.File]::ReadAllText($env:SHELL_BACKUP) -eq 'custom-original-shell.exe')
. $env:TEST_SOURCE -Action restore
. $env:TEST_SOURCE -Action restore
Check ($script:current -eq 'custom-original-shell.exe')
$script:failNext=$true
$failed=$false;try{. $env:TEST_SOURCE -Action install}catch{$failed=$true}
Check ($failed -and $script:current -eq 'custom-original-shell.exe')
[IO.File]::WriteAllText($env:SHELL_BACKUP,'')
$failed=$false;try{. $env:TEST_SOURCE -Action install}catch{$failed=$true}
Check ($failed -and $script:current -eq 'custom-original-shell.exe')
[IO.File]::WriteAllBytes($env:SHELL_BACKUP,[byte[]]@(255,254,255))
$failed=$false;try{. $env:TEST_SOURCE -Action restore}catch{$failed=$true}
Check ($failed -and $script:current -eq 'custom-original-shell.exe')
# Exercise automatic native-host preference and quoted script fallback, without running either.
[IO.File]::WriteAllText($env:SHELL_BACKUP,'custom-original-shell.exe')
$env:CRAFT_ROOT=$env:TEST_DIRECTORY;$env:CRAFT_BIN=Join-Path $env:TEST_DIRECTORY 'bin'
[IO.Directory]::CreateDirectory($env:CRAFT_BIN)|Out-Null
[IO.File]::WriteAllText((Join-Path $env:CRAFT_ROOT 'session-shell.cmd'),'fixture')
[IO.File]::WriteAllText((Join-Path $env:CRAFT_BIN 'plasmashell.exe'),'fixture')
$native=Join-Path $env:CRAFT_BIN 'kde-session-host.exe';[IO.File]::WriteAllText($native,'fixture')
$env:SHELL_VALUE=''
. $env:TEST_SOURCE -Action install
Check ($script:current -eq ([char]34+$native+[char]34))
[IO.File]::Delete($native)
. $env:TEST_SOURCE -Action install
Check ($script:current.StartsWith('cmd.exe /d /s /c ""') -and $script:current.EndsWith('""'))
$before=$script:current;[IO.File]::Delete((Join-Path $env:CRAFT_ROOT 'session-shell.cmd'))
$failed=$false;try{. $env:TEST_SOURCE -Action install}catch{$failed=$true};Check ($failed -and $script:current -eq $before)
[IO.File]::Delete($env:SHELL_BACKUP)
$script:current='"C:\KDE\bin\kde-session-host.exe"'
$env:SHELL_VALUE='new-shell.exe'
$failed=$false;try{. $env:TEST_SOURCE -Action install}catch{$failed=$true}
Check ($failed -and -not [IO.File]::Exists($env:SHELL_BACKUP) -and $script:current.Contains('kde-session-host.exe'))
Write-Output 'PASS: production Shell backup preserved on reinstall; repeated restore; write rollback; empty/corrupt backup rejected; no registry changes.'
''',encoding='utf-8')
    env=os.environ.copy();env.update(TEST_DIRECTORY=str(folder),TEST_SOURCE=str(root/'upstream/plasma-windows/tools/shell-registry.ps1'))
    result=subprocess.run(['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File',str(driver)],env=env,capture_output=True,timeout=25)
    print(result.stdout.decode(errors='replace'));assert result.returncode==0,(result.stdout,result.stderr)
