# SPDX-License-Identifier: GPL-3.0-or-later
"""Ship desktop Qt QML module files and their SDK DLL closure, even if in the base ZIP."""
import argparse,json
from pathlib import Path
from pe_imports import imports,architecture
parser=argparse.ArgumentParser()
parser.add_argument('--runtime',type=Path,default=Path('D:/KDE/CraftRoot'))
parser.add_argument('--manifest',type=Path)
parser.add_argument('--check-only',action='store_true')
args=parser.parse_args()
root=Path(__file__).resolve().parents[1]
manifest_path=args.manifest or root/'manifests/cumulative-patch-files.json'
manifest=json.loads(manifest_path.read_text(encoding='utf-8'))
plan=json.loads((root/'manifests/qt-qml-runtime-roots.json').read_text(encoding='utf-8'))
runtime=args.runtime.resolve()
missing=[name for name in plan['required'] if not (runtime/name).is_file()]
if missing:raise SystemExit('Missing essential desktop QML runtime files: '+', '.join(missing))
files={p for name in plan['roots'] for p in (runtime/'qml'/name).rglob('*') if p.is_file()}
if plan['include_qtquick_root_files']:files.update(p for p in (runtime/'qml/QtQuick').iterdir() if p.is_file())
qt_files=len(files)
files.update(runtime/name for name in plan.get('extra_plugins',[]))
dlls={p.name.lower():p for p in (runtime/'bin').glob('*.dll')}
queue=[p for p in files if p.suffix.lower()=='.dll'];seen=set()
while queue:
    module=queue.pop()
    if module in seen:continue
    seen.add(module)
    if architecture(module)!='x64':raise SystemExit('Unexpected architecture in desktop QML runtime: '+str(module))
    for name in imports(module):
        dependency=dlls.get(name.lower())
        if dependency is not None:files.add(dependency);queue.append(dependency)
required={p.relative_to(runtime).as_posix() for p in files}|set(plan['required'])
added=sorted(required-set(manifest['runtime']))
if args.check_only:
    if added:raise SystemExit('Desktop QML runtime omitted from cumulative overlay: '+', '.join(added))
else:
    manifest['runtime']=sorted(set(manifest['runtime'])|required)
    manifest_path.write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'qml_files':qt_files,'modules_checked':len(seen),'required_overlay_files':len(required),'added_files':len(added)},indent=2))
