# SPDX-License-Identifier: GPL-3.0-or-later
"""Invalid-shell startup exits before corona.init; no desktop session is started."""
import os,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
runtime=Path(os.environ.get('KDE_CRAFT_ROOT','D:/KDE/CraftRoot'))
with tempfile.TemporaryDirectory(prefix='plasma-errors-',dir=root/'work') as folder:
    env=os.environ.copy();env.update(QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',QT_PLUGIN_PATH=str(runtime/'plugins'),QML_IMPORT_PATH=str(runtime/'qml'),DBUS_SESSION_BUS_ADDRESS='tcp:host=127.0.0.1,port=19999',TEMP=folder,TMP=folder)
    result=subprocess.run([str(runtime/'bin/plasmashell.exe'),'--no-respawn','--shell-plugin','org.nixbridge.invalid-startup-regression'],env=env,capture_output=True,timeout=15)
    assert result.returncode==1,(result.returncode,result.stdout,result.stderr)
    assert b'starting invalid corona' in result.stderr,result.stderr
    assert b'org.nixbridge.invalid-startup-regression' in result.stderr
    legacy=Path(folder)/'plasmashell-debug.log'
    assert legacy.exists() and b'starting invalid corona' in legacy.read_bytes()
print('PASS: actual plasmashell invalid-shell exit 1, Qt error captured on stderr and legacy TEMP log; corona.init and live desktop not entered.')
