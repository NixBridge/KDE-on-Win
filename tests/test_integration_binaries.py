# SPDX-License-Identifier: GPL-3.0-or-later
"""Check actual native PE bitness and independently deployed module imports."""
import os
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'tools'))
from pe_imports import architecture, imports
binary=Path(os.environ.get('KDE_CRAFT_ROOT', 'D:/KDE/CraftRoot')) / 'bin'
for relative,bits in [('kde-controls.dll','x64'),('kde-style-host.exe','x64'),('x86/kde-controls.dll','x86'),('x86/kde-style-host.exe','x86'),('kde-logon-provider.dll','x64'),('kde-logon-probe.exe','x64'),('kde-session-host.exe','x64')]:
    module=binary/relative
    assert architecture(module)==bits,(relative,bits)
    dependencies=[name.lower() for name in imports(module)]
    assert dependencies and not any(name.startswith(('qt','msvcp','vcruntime')) for name in dependencies),(relative,dependencies)
assert 'secur32.dll' in [name.lower() for name in imports(binary/'kde-logon-provider.dll')]
print('PASS: actual x86/x64 integration modules match intended architectures and need neither Qt nor a separately installed MSVC runtime.')
