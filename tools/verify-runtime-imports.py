"""Check x64 PE imports of the runtime entry points without starting the shell."""
import argparse
import json
import os
import struct
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--root', type=Path, required=True)
args = parser.parse_args()
runtime = args.root.resolve()
binary_dir = runtime / 'bin'
system_dir = Path(os.environ['SystemRoot']) / 'System32'
files = {p.name.lower(): p for p in binary_dir.glob('*.dll')}

def imports(path):
    data = path.read_bytes()
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    assert data[pe:pe+4] == b'PE\0\0', path
    machine, section_count = struct.unpack_from('<HH', data, pe+4)
    assert machine == 0x8664, f'Not x64: {path}'
    optional_size = struct.unpack_from('<H', data, pe+20)[0]
    optional = pe + 24
    assert struct.unpack_from('<H', data, optional)[0] == 0x20b, path
    import_rva = struct.unpack_from('<I', data, optional+112+8)[0]
    section_base = optional + optional_size
    sections = [struct.unpack_from('<IIII', data, section_base+i*40+8) for i in range(section_count)]
    def offset(rva):
        for virtual_size, address, raw_size, raw_offset in sections:
            if address <= rva < address + max(virtual_size, raw_size):
                return raw_offset + rva - address
        raise ValueError(f'Invalid RVA {rva:x}: {path}')
    if not import_rva:
        return []
    cursor = offset(import_rva)
    result = []
    while any(data[cursor:cursor+20]):
        name_rva = struct.unpack_from('<I', data, cursor+12)[0]
        start = offset(name_rva)
        end = data.index(b'\0', start)
        result.append(data[start:end].decode('ascii'))
        cursor += 20
    return result

queue = [binary_dir / name for name in ['plasmashell.exe', 'krunner.exe', 'kded6.exe', 'kactivitymanagerd.exe', 'dolphin.exe', 'dbus-daemon.exe', 'trayhost.exe', 'shellswitch.exe', 'kwindowsystem-probe.exe']]
queue += [runtime / relative for relative in ['plugins/platforms/qwindows.dll', 'plugins/sqldrivers/qsqlite.dll', 'qml/org/kde/plasma/windowsmenu/startmenuplugin.dll']]
for directory in ['qml', 'plugins']:
    queue.extend((runtime / directory).rglob('*.dll'))
seen = set()
missing = []
while queue:
    path = queue.pop()
    key = str(path).lower()
    if key in seen:
        continue
    seen.add(key)
    if not path.exists():
        missing.append({'module': str(path), 'missing': '<entry point>'})
        continue
    for name in imports(path):
        key = name.lower()
        if key.startswith(('api-ms-', 'ext-ms-')):
            continue
        dependency = files.get(key)
        if dependency:
            queue.append(dependency)
        elif (path.parent / name).exists():
            queue.append(path.parent / name)
        elif (system_dir / name).exists():
            continue
        else:
            missing.append({'module': str(path), 'missing': name})
report = {'modules_checked': len(seen), 'architecture': 'x64', 'missing': missing}
(Path(__file__).resolve().parents[1] / 'manifests' / 'runtime-import-verification.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps(report, indent=2))
raise SystemExit(1 if missing else 0)
