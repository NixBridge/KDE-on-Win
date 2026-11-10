# SPDX-License-Identifier: GPL-3.0-or-later
# Canonical current-user Shell write; callers provide values via environment.
param([Parameter(Mandatory=$true)][ValidateSet('install','restore')][string]$Action,[string]$ShellPath)
$ErrorActionPreference='Stop'
if(-not $env:SHELL_KEY_PS -or -not $env:SHELL_BACKUP){throw 'Shell registry key and backup path are required.'}
if(-not $env:SHELL_KEY_PS.StartsWith('HKCU:\',[StringComparison]::OrdinalIgnoreCase)){throw 'Only current-user Shell keys are supported.'}
$backupDirectory=[IO.Path]::GetDirectoryName($env:SHELL_BACKUP)
[IO.Directory]::CreateDirectory($backupDirectory)|Out-Null
# Serialize every entrypoint using this backup; a failed concurrent install leaves the first intact.
$lock=[IO.File]::Open($env:SHELL_BACKUP+'.lock',[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
try {
    $previous='explorer.exe'
    if(Test-Path -LiteralPath $env:SHELL_KEY_PS){
        $item=Get-ItemProperty -LiteralPath $env:SHELL_KEY_PS
        if($item.PSObject.Properties['Shell'] -and -not [string]::IsNullOrWhiteSpace($item.Shell)){$previous=[string]$item.Shell}
    }
    $value='explorer.exe'
    if($Action -eq 'install'){
        $value=$env:SHELL_VALUE
        if(-not [string]::IsNullOrWhiteSpace($ShellPath)){
            if(-not [IO.File]::Exists($ShellPath)){throw 'The selected Shell executable is missing; Shell was not changed.'}
            $value=[char]34+[IO.Path]::GetFullPath($ShellPath)+[char]34
        }
        if([string]::IsNullOrWhiteSpace($value)){
            $script=Join-Path $env:CRAFT_ROOT 'session-shell.cmd'
            $native=Join-Path $env:CRAFT_BIN 'kde-session-host.exe'
            if(-not [IO.File]::Exists($script) -or -not [IO.File]::Exists((Join-Path $env:CRAFT_BIN 'plasmashell.exe'))){throw 'The session runtime is incomplete; Shell was not changed.'}
            $quote=[char]34
            if([IO.File]::Exists($native)){$value=$quote+$native+$quote}
            else{$value='cmd.exe /d /s /c '+$quote+$quote+$script+$quote+$quote}
        }
        if(-not [IO.File]::Exists($env:SHELL_BACKUP)){
            if($previous -match '(?i)(?:^|[\\/\s"])(?:kde-session-host\.exe|plasmashell\.exe|session-shell\.cmd)(?:[\s"]|$)'){
                throw 'The current Shell is already KDE-on-Win but its original backup is missing. Restore Explorer or review the configuration before reinstalling.'
            }
            $stream=[IO.File]::Open($env:SHELL_BACKUP,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
            try{$bytes=[Text.UTF8Encoding]::new($false).GetBytes($previous);$stream.Write($bytes,0,$bytes.Length);$stream.Flush($true)}finally{$stream.Dispose()}
        }
        # Reject a corrupt/empty backup instead of installing without a usable recovery target.
        $saved=[IO.File]::ReadAllText($env:SHELL_BACKUP,[Text.UTF8Encoding]::new($false,$true))
        if([string]::IsNullOrWhiteSpace($saved)){throw 'The original Shell backup is empty; review it before installing.'}
    } elseif([IO.File]::Exists($env:SHELL_BACKUP)){
        $value=[IO.File]::ReadAllText($env:SHELL_BACKUP,[Text.UTF8Encoding]::new($false,$true))
        if([string]::IsNullOrWhiteSpace($value)){throw 'The Shell backup is empty; restore aborted without changing the registry.'}
    }
    try{
        # Registry New-Item -Force destroys an existing key's other values/subkeys.
        # Create only a missing key, never recreate an existing Winlogon key.
        if(-not (Test-Path -LiteralPath $env:SHELL_KEY_PS)){New-Item -Path $env:SHELL_KEY_PS|Out-Null}
        New-ItemProperty -LiteralPath $env:SHELL_KEY_PS -Name Shell -Value $value -PropertyType String -Force|Out-Null
    } catch {
        $failure=$_
        try{New-ItemProperty -LiteralPath $env:SHELL_KEY_PS -Name Shell -Value $previous -PropertyType String -Force|Out-Null}
        catch{Write-Warning 'Shell write and rollback both failed. The original backup is retained for manual recovery.'}
        throw $failure
    }
    Write-Output ('Shell '+$Action+' completed. Original backup retained.')
} finally {$lock.Dispose()}
