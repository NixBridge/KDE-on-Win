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
assert not any('UserChoice' in r['Path'] or '\\Folder\\shell' in r['Path'] for r in records)
commands=[r['Value'] for r in records if r['Path'].endswith('nixbridge-dolphin\\command')]
assert len(commands)==2
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
        assert len(args)==3 and args[1]=='dolphin',args
        assert ntpath.normpath(args[2])==ntpath.normpath(directory),(directory,args)
print('PASS: association plan preserves UserChoice/namespace folders; Windows argv parsing preserves drive roots, UNC roots, spaces and Unicode. No registry values changed.')
