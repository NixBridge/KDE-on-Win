# SPDX-License-Identifier: GPL-3.0-or-later
[CmdletBinding()]
param([string]$CraftRoot=$PSScriptRoot,[switch]$Restore,[switch]$Plan,[switch]$IfNeeded)
$ErrorActionPreference='Stop'
$CraftRoot=[IO.Path]::GetFullPath($CraftRoot)
if($CraftRoot.Length -gt [IO.Path]::GetPathRoot($CraftRoot).Length){$CraftRoot=$CraftRoot.TrimEnd([char[]]'\/')}
$launcher=Join-Path $CraftRoot 'bin\plasma-kde-launcher.exe'
if(-not (Test-Path -LiteralPath $launcher -PathType Leaf)){throw "Missing launcher: $launcher"}
$dolphin=Join-Path $CraftRoot 'bin\dolphin.exe'
if(-not (Test-Path -LiteralPath $dolphin -PathType Leaf)){throw "Missing Dolphin: $dolphin"}
$changes=[Collections.Generic.List[object]]::new()
function Add-Value([string]$Path,[string]$Name,[string]$Value){$changes.Add([pscustomobject]@{Path=$Path;Name=$Name;Value=$Value})}
$classes='Software\Classes'
foreach($type in @('Directory','Drive','Folder')){
    Add-Value "$classes\$type\shell" '' 'nixbridge-dolphin'
    foreach($verb in @('nixbridge-dolphin','open','explore','opennewwindow','opennewprocess')){
        Add-Value "$classes\$type\shell\$verb" '' 'Open with Dolphin'
        Add-Value "$classes\$type\shell\$verb" 'Icon' ('"'+(Join-Path $CraftRoot 'bin\dolphin.exe')+'",0')
        # The suffix safely quotes drive roots. The launcher also understands
        # shell namespace names and strips the suffix before parsing those.
        Add-Value "$classes\$type\shell\$verb\command" '' ('"'+$launcher+'" folder "%1\."')
        # Override Explorer's inherited COM verb handler for explicit opens.
        Add-Value "$classes\$type\shell\$verb\command" 'DelegateExecute' ''
    }
}
$prog='NixBridge.KDEArchive'
Add-Value "$classes\$prog" '' 'KDE Archive'
Add-Value "$classes\$prog\DefaultIcon" '' ('"'+(Join-Path $CraftRoot 'bin\plasma-archive-manager.exe')+'",0')
Add-Value "$classes\$prog\shell\open\command" '' ('"'+$launcher+'" archive "%1"')
$extensions=@('.zip','.7z','.tar','.tgz','.tbz2','.txz')
foreach($extension in $extensions){
    Add-Value "$classes\$extension\OpenWithProgids" $prog ''
    # UserChoice is deliberately untouched. This is the fallback association.
    Add-Value "$classes\$extension" '' $prog
    Add-Value 'Software\NixBridge\KDE-on-Win\Capabilities\FileAssociations' $extension $prog
}
Add-Value 'Software\NixBridge\KDE-on-Win\Capabilities' 'ApplicationName' 'KDE Archive Manager'
Add-Value 'Software\NixBridge\KDE-on-Win\Capabilities' 'ApplicationDescription' 'Open ZIP, 7z and TAR archives in KDE-on-Win'
Add-Value 'Software\RegisteredApplications' 'KDE-on-Win Archive Manager' 'Software\NixBridge\KDE-on-Win\Capabilities'
if($Plan){$changes | ConvertTo-Json -Depth 5;return}
$state=Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'plasma-win-session'
[IO.Directory]::CreateDirectory($state)|Out-Null
$backupPath=Join-Path $state 'file-associations-backup.json'
$marker=Join-Path $state 'file-associations-version.txt'
$signature=$CraftRoot.ToUpperInvariant()+'#2'
if(-not $Restore -and $IfNeeded -and (Test-Path -LiteralPath $marker) -and ([IO.File]::ReadAllText($marker) -ceq $signature)){return}
$records=[Collections.Generic.List[object]]::new()
if(Test-Path -LiteralPath $backupPath){foreach($r in @(Get-Content -LiteralPath $backupPath -Raw | ConvertFrom-Json)){$records.Add($r)}}
$registry=[Microsoft.Win32.Registry]::CurrentUser
if($Restore){
    foreach($r in $records){$key=$registry.OpenSubKey($r.Path,$true);if(-not $key){continue};try{
        # Keep changes made by the user or another application after install.
        if($key.GetValue($r.Name,$null,[Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames) -cne $r.Written){continue}
        if($r.Existed){$key.SetValue($r.Name,$r.Original,[Microsoft.Win32.RegistryValueKind]([Enum]::Parse([Microsoft.Win32.RegistryValueKind],$r.Kind)))}else{$key.DeleteValue($r.Name,$false)}
    }finally{$key.Dispose()}}
    $mimeProcess=Start-Process -FilePath $launcher -ArgumentList '--restore-mime-defaults' -Wait -PassThru -WindowStyle Hidden
    if($mimeProcess.ExitCode -ne 0){throw 'Could not restore KDE MIME defaults.'}
    Write-Output 'Original per-user association values restored where still owned by KDE-on-Win.'
}else{
    foreach($change in $changes){
        $record=@($records | Where-Object {$_.Path -ceq $change.Path -and $_.Name -ceq $change.Name}) | Select-Object -First 1
        if(-not $record){
            $key=$registry.OpenSubKey($change.Path);$exists=$false;$original=$null;$kind='String'
            if($key){try{$exists=$key.GetValueNames() -contains $change.Name;if($exists){$original=$key.GetValue($change.Name,$null,[Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames);$kind=$key.GetValueKind($change.Name).ToString()}}finally{$key.Dispose()}}
            $record=[pscustomobject]@{Path=$change.Path;Name=$change.Name;Existed=$exists;Original=$original;Kind=$kind;Written=$change.Value};$records.Add($record)
        }else{$record.Written=$change.Value}
        # Persist the undo record before changing any association.
        $temporary=$backupPath+'.tmp';[IO.File]::WriteAllText($temporary,($records|ConvertTo-Json -Depth 6),[Text.UTF8Encoding]::new($false));if([IO.File]::Exists($backupPath)){[IO.File]::Replace($temporary,$backupPath,$null)}else{[IO.File]::Move($temporary,$backupPath)}
        $key=$registry.CreateSubKey($change.Path);try{$key.SetValue($change.Name,$change.Value,[Microsoft.Win32.RegistryValueKind]::String)}finally{$key.Dispose()}
    }
    $mimeProcess=Start-Process -FilePath $launcher -ArgumentList '--register-mime-defaults' -Wait -PassThru -WindowStyle Hidden
    if($mimeProcess.ExitCode -ne 0){throw 'Could not write KDE MIME defaults.'}
    Write-Output 'Dolphin directory defaults and KDE archive fallbacks installed. Existing protected file defaults require Open With > Always in the VM.'
}
[IO.File]::WriteAllText($marker,$signature,[Text.UTF8Encoding]::new($false))
Add-Type -TypeDefinition 'using System;using System.Runtime.InteropServices;public static class KDEAssociationChange{[DllImport("shell32.dll")]public static extern void SHChangeNotify(uint e,uint f,IntPtr a,IntPtr b);}'
[KDEAssociationChange]::SHChangeNotify(0x08000000,0,[IntPtr]::Zero,[IntPtr]::Zero)
