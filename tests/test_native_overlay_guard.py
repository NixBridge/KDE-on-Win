# SPDX-License-Identifier: GPL-3.0-or-later
import os,subprocess,sys,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
runtime=Path(os.environ.get('KDE_CRAFT_ROOT','D:/KDE/CraftRoot')).resolve()
info=runtime/'bin/data/kde-on-win/build-info.json'
before=info.read_bytes() if info.exists() else None
with tempfile.TemporaryDirectory(prefix='native-overlay-',dir=root/'work') as temp:
    manifest=Path(temp)/'install.txt'
    for product,expected in [(runtime/'bin/omitted-regression-fixture.exe',b'missing from cumulative overlay'),(Path(temp)/'outside.dll',b'outside the selected runtime')]:
        manifest.write_text(str(product)+'\n',encoding='utf-8')
        result=subprocess.run([sys.executable,str(root/'tools/make-cumulative-patch.py'),'--runtime',str(runtime),'--version','guard-fixture','--native-install-manifest',str(manifest)],capture_output=True,timeout=10)
        assert result.returncode!=0 and expected in result.stderr,(result.stdout,result.stderr)
assert (info.read_bytes() if info.exists() else None)==before
assert not (root/'dist/guard-fixture').exists()
print('PASS: omitted/outside native products reject packaging before release stamping or ZIP creation; runtime metadata unchanged.')
