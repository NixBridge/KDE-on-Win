# SPDX-License-Identifier: GPL-3.0-or-later
"""Validate the non-mutating association plan and real Windows argv parsing."""
import ctypes
import json
import ntpath
import subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=subprocess.run(['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File',str(root/'runtime/install-file-associations.ps1'),'-CraftRoot','D:/KDE/CraftRoot','-Plan'],capture_output=True,check=True)
records=json.loads(p.stdout.decode('utf-8'))
assert not any('UserChoice' in r['Path'] for r in records)
commands=[r['Value'] for r in records if r['Path'].endswith('nixbridge-dolphin\\command') and r['Name']=='']
assert len(commands)==3
assert any(r['Path'].endswith('KDEOpenWith\\Capabilities\\URLAssociations') and r['Name']=='mailto' and r['Value']=='NixBridge.KDEOpenWith' for r in records)
assert any(r['Path']=='Software\\Classes\\mailto\\shell\\open\\command' and 'plasma-open-with.exe' in r['Value'] for r in records)
for kind in ['Directory','Drive','Folder']:
    for verb in ['nixbridge-dolphin','open','explore','opennewwindow','opennewprocess']:
        key='Software\\Classes\\'+kind+'\\shell\\'+verb+'\\command'
        assert any(r['Path']==key and r['Name']=='DelegateExecute' and r['Value']=='' for r in records)
parse=ctypes.windll.shell32.CommandLineToArgvW
parse.argtypes=[ctypes.c_wchar_p,ctypes.POINTER(ctypes.c_int)]
parse.restype=ctypes.POINTER(ctypes.c_wchar_p)
free=ctypes.windll.kernel32.LocalFree
free.argtypes=[ctypes.c_void_p]
for command in commands:
    for directory in ['C:\\','D:\\space name\\child','D:\\a&b\\','\\\\server\\share\\','D:\\unicode-\u6d4b\u8bd5\\']:
        count=ctypes.c_int()
        argv=parse(command.replace('%1',directory),ctypes.byref(count))
        assert argv
        try: args=[argv[i] for i in range(count.value)]
        finally: free(ctypes.cast(argv,ctypes.c_void_p))
        assert len(args)==3 and args[1]=='folder',args
        assert ntpath.normpath(args[2])==ntpath.normpath(directory),(directory,args)
print('PASS: association plan covers Directory/Drive/Folder and all open verbs, preserves UserChoice; Windows argv parsing preserves drive roots, UNC roots, spaces and Unicode. No registry values changed.')
