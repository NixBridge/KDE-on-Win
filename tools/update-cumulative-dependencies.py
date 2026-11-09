# SPDX-License-Identifier: GPL-3.0-or-later
"""Add transitive DLLs absent from the original runtime to the overlay manifest."""
import argparse
import json
import zipfile
from pathlib import Path
from pe_imports import imports, architecture

parser=argparse.ArgumentParser()
parser.add_argument('--runtime',type=Path,default=Path('D:/KDE/CraftRoot'))
parser.add_argument('--base-archive',type=Path,required=True)
args=parser.parse_args()
root=Path(__file__).resolve().parents[1]
manifest_path=root/'manifests/cumulative-patch-files.json'
manifest=json.loads(manifest_path.read_text())
with zipfile.ZipFile(args.base_archive) as archive:
    original=set(archive.namelist())
included=set(manifest['runtime'])|set(manifest['repository'])
dlls={p.name.lower():p for p in (args.runtime/'bin').glob('*.dll')}
x86_dlls={p.name.lower():p for p in (args.runtime/'bin/x86').glob('*.dll')}
queue=[args.runtime/relative for relative in manifest['runtime'] if Path(relative).suffix.lower() in ['.exe','.dll']]
seen=set()
added=set()
while queue:
    module=queue.pop()
    if module in seen:continue
    seen.add(module)
    candidates=x86_dlls if architecture(module)=='x86' else dlls
    for name in imports(module):
        dependency=candidates.get(name.lower())
        if dependency is None:continue
        relative=dependency.relative_to(args.runtime).as_posix()
        if relative not in original and relative not in included:added.add(relative)
        queue.append(dependency)
manifest['runtime']=sorted(set(manifest['runtime'])|added)
manifest_path.write_text(json.dumps(manifest,indent=2)+'\n')
print(json.dumps({'modules_checked':len(seen),'new_dlls':sorted(added)},indent=2))
