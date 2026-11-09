# SPDX-License-Identifier: GPL-3.0-or-later
# Functions only; dot-sourcing this file performs no deployment.
function Get-KdeProgramFiles64 {
    $base=[Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::LocalMachine,[Microsoft.Win32.RegistryView]::Registry64)
    try{
        $key=$base.OpenSubKey('Software\Microsoft\Windows\CurrentVersion')
        try{$path=[string]$key.GetValue('ProgramFilesDir')}finally{if($key){$key.Dispose()}}
    }finally{$base.Dispose()}
    if([string]::IsNullOrWhiteSpace($path) -or -not [IO.Path]::IsPathRooted($path)){throw 'Cannot resolve the native Program Files directory.'}
    return [IO.Path]::GetFullPath($path)
}
function Assert-KdePlainPath([string]$Path) {
    $resolved=[IO.Path]::GetFullPath($Path)
    $cursor=$resolved
    while(-not [string]::IsNullOrEmpty($cursor)){
        try{
            $attributes=[IO.File]::GetAttributes($cursor)
            if($attributes -band [IO.FileAttributes]::ReparsePoint){throw "Refusing reparse point: $cursor"}
        }catch [IO.FileNotFoundException]{}catch [IO.DirectoryNotFoundException]{}
        $parent=[IO.Path]::GetDirectoryName($cursor)
        if($parent -eq $cursor){break}
        $cursor=$parent
    }
    return $resolved
}
function Get-KdeBinaryHash([string]$Path) {
    $stream=[IO.File]::OpenRead($Path)
    $algorithm=$null
    try{
        $algorithm=[Security.Cryptography.SHA256]::Create()
        return [BitConverter]::ToString($algorithm.ComputeHash($stream)).Replace('-','')
    }finally{if($algorithm){$algorithm.Dispose()};$stream.Dispose()}
}
function Publish-KdeBinary([string]$Source,[string]$Target,[scriptblock]$PrepareFile) {
    $targetPath=Assert-KdePlainPath $Target
    $directory=[IO.Path]::GetDirectoryName($targetPath)
    if(-not [IO.Directory]::Exists($directory)){throw 'The deployment directory must already exist.'}
    $expected=Get-KdeBinaryHash $Source
    $pending=Join-Path $directory ('.kde-pending-'+[Guid]::NewGuid().ToString('N')+'.dll')
    try{
        [IO.File]::Copy($Source,$pending,$false)
        if($PrepareFile){& $PrepareFile $pending}
        if((Get-KdeBinaryHash $pending) -cne $expected){throw 'Staged binary hash mismatch.'}
        $null=Assert-KdePlainPath $targetPath
        if([IO.File]::Exists($targetPath)){
            [IO.File]::Replace($pending,$targetPath,[NullString]::Value)
        }else{
            [IO.File]::Move($pending,$targetPath)
        }
    }finally{
        if([IO.File]::Exists($pending)){[IO.File]::Delete($pending)}
    }
}
