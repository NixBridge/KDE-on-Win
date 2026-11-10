# SPDX-License-Identifier: GPL-3.0-or-later
"""Merge a verified cumulative overlay into the preserved original VM package."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import zipfile

parser = argparse.ArgumentParser()
parser.add_argument('--base', type=Path, required=True)
parser.add_argument('--overlay', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
if args.output.resolve() in [args.base.resolve(), args.overlay.resolve()]:
    parser.error('output must differ from both inputs')
args.output.parent.mkdir(parents=True, exist_ok=True)
partial = args.output.with_suffix('.partial.zip')
verified = False
try:
    with zipfile.ZipFile(args.base) as base, zipfile.ZipFile(args.overlay) as overlay:
        overlay_names = set(overlay.namelist())
        for archive in [base, overlay]:
            if len(archive.namelist()) != len(set(archive.namelist())):
                raise ValueError('duplicate archive entries')
            for name in archive.namelist():
                if '\\' in name or name.startswith('/') or ':' in name or '..' in name.split('/'):
                    raise ValueError('unsafe archive path: ' + name)
        with zipfile.ZipFile(partial, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=3) as out:
            for archive in [base, overlay]:
                for entry in archive.infolist():
                    if archive is base and entry.filename in overlay_names:
                        continue
                    with archive.open(entry) as source, out.open(entry.filename, 'w', force_zip64=True) as target:
                        shutil.copyfileobj(source, target, 1024 * 1024)
        with zipfile.ZipFile(partial) as merged:
            assert merged.testzip() is None
            assert set(merged.namelist()) == set(base.namelist()) | set(overlay.namelist())
            for entry in overlay.infolist():
                assert merged.read(entry.filename) == overlay.read(entry.filename), entry.filename
        verified = True
    partial.replace(args.output)
except BaseException:
    if not verified:
        partial.unlink(missing_ok=True)
    else:
        print('Verified package retained at ' + str(partial) + '; close readers before publishing it.')
    raise
with args.output.open('rb') as stream:
    digest = hashlib.file_digest(stream, 'sha256').hexdigest()
Path(str(args.output) + '.sha256').write_text(digest + ' *' + args.output.name + '\n', encoding='utf-8')
print(json.dumps({'archive': str(args.output.resolve()), 'bytes': args.output.stat().st_size,
                  'sha256': digest}, indent=2))
