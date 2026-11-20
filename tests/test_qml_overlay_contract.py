# SPDX-License-Identifier: GPL-3.0-or-later
import json,os,subprocess,sys,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
runtime=Path(os.environ.get('KDE_CRAFT_ROOT','D:/KDE/CraftRoot'))
command=[sys.executable,str(root/'tools/ensure-qml-runtime.py'),'--runtime',str(runtime),'--check-only']
p=subprocess.run(command,capture_output=True,timeout=20)
assert p.returncode==0,(p.stdout,p.stderr)
original=root/'manifests/cumulative-patch-files.json';before=original.read_bytes()
manifest=json.loads(before)
with tempfile.TemporaryDirectory(prefix='qml-overlay-',dir=root/'work') as temp:
    fixture=Path(temp)/'manifest.json'
    for missing in ('qml/QtQuick/Layouts/qmldir', 'qml/org/kde/notificationmanager/qmldir',
                    'plugins/plasma/applets/org.kde.plasma.imewin.dll'):
        manifest['runtime'].remove(missing)
        fixture.write_text(json.dumps(manifest),encoding='utf-8')
        p=subprocess.run(command+['--manifest',str(fixture)],capture_output=True,timeout=20)
        assert p.returncode!=0 and missing.encode() in p.stderr,(p.stdout,p.stderr)
        manifest['runtime'].append(missing)
assert original.read_bytes()==before
print('PASS: desktop Qt/KDE QML trees and native applets; deliberate Qt, KDE and IME overlay omissions rejected.')
