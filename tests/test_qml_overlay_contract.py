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
manifest['runtime'].remove('qml/QtQuick/Layouts/qmldir')
with tempfile.TemporaryDirectory(prefix='qml-overlay-',dir=root/'work') as temp:
    fixture=Path(temp)/'manifest.json';fixture.write_text(json.dumps(manifest),encoding='utf-8')
    p=subprocess.run(command+['--manifest',str(fixture)],capture_output=True,timeout=20)
    assert p.returncode!=0 and b'qml/QtQuick/Layouts/qmldir' in p.stderr,(p.stdout,p.stderr)
assert original.read_bytes()==before
print('PASS: desktop Qt QML trees, plugin/DLL closure and essential module overlay coverage; deliberate Layouts omission rejected, manifest unchanged.')
