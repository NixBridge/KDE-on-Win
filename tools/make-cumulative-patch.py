# SPDX-License-Identifier: GPL-3.0-or-later
"""Package every runtime fix needed on top of the original VM archive."""
import argparse
import hashlib
import json
import shutil
import zipfile
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--runtime', type=Path, default=Path('D:/KDE/CraftRoot'))
parser.add_argument('--version', required=True)
args = parser.parse_args()
if not args.version.replace('-', '').isalnum():
    parser.error('version must contain only letters, digits and hyphens')
root = Path(__file__).resolve().parents[1]
manifest = json.loads((root / 'manifests' / 'cumulative-patch-files.json').read_text())
out = root / 'dist' / args.version
stage = out / 'patch'
sources = {relative: args.runtime / relative for relative in manifest['runtime']}
sources.update({relative: root / source for relative, source in manifest['repository'].items()})
missing = [str(source) for source in sources.values() if not source.is_file()]
if missing:
    raise SystemExit('Missing cumulative patch files: ' + ', '.join(missing))
for relative, source in sources.items():
    target = stage / relative
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, target)
(stage / 'README-CUMULATIVE.txt').write_text(
    'Cumulative overlay for the ORIGINAL Plasma Windows VM archive. Includes all previous fixes.\n'
    'Restart the VM, extract into the existing CraftRoot and overwrite files. Start deploy-vm.cmd.\n'
    'Desktop and launcher use KDE Breeze icons. Windows shortcuts open through the Shell, preserving their arguments.\n'
    'Only the Plasma panel is kept above Explorer taskbars; the desktop wallpaper is not made topmost.\n'
    'Run apply-panel-opacity.cmd after startup if the existing panel is transparent.\n'
    'Built-in Windows programs use equivalent Breeze icons; QWidget context menus use an opaque topmost Windows path.\n'
    'Full VM interaction and stacking acceptance still require testing.\n', encoding='utf-8')
archive = out / ('plasma-windows-' + args.version + '.zip')
with zipfile.ZipFile(archive, 'w', compression=zipfile.ZIP_DEFLATED) as z:
    for file in sorted(stage.rglob('*')):
        if file.is_file():
            z.write(file, file.relative_to(stage).as_posix())
with zipfile.ZipFile(archive) as z:
    assert set(sources) <= set(z.namelist())
    assert z.testzip() is None
with archive.open('rb') as stream:
    digest = hashlib.file_digest(stream, 'sha256').hexdigest()
Path(str(archive) + '.sha256').write_text(digest + ' *' + archive.name + '\n', encoding='utf-8')
print(json.dumps({'archive': str(archive), 'bytes': archive.stat().st_size,
                  'sha256': digest, 'cumulative_runtime_files': len(sources)}, indent=2))
