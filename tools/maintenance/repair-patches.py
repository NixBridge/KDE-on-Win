"""Regenerate malformed imported patches from diffs against release sources."""
import json
import re
import subprocess
import tarfile
from pathlib import Path

root = Path(__file__).resolve().parents[2]
port = root / 'upstream/plasma-windows'
store = Path('D:/KDE/sources')
manifest = json.loads((port / 'upstream-versions.json').read_text())
patch_exe = 'C:/Program Files/Git/usr/bin/patch.exe'

def run(argv, cwd, **kw):
    return subprocess.run(argv, cwd=cwd, check=True, **kw)

def normalize(text):
    lines = text.splitlines(keepends=True)
    out = []
    for i, line in enumerate(lines):
        out.append(line)
        if line.startswith('diff --git ') and i + 1 < len(lines) and lines[i+1].startswith('@@'):
            a, b = line.strip().split()[2:4]
            out.extend([f'--- {a}\n', f'+++ {b}\n'])
    lines = out
    for i, line in enumerate(lines):
        match = re.match(r'@@ -(\d+)(?:,\d+)? \+(\d+)(?:,\d+)? @@(.*)', line)
        if not match:
            continue
        old = new = 0
        for body in lines[i+1:]:
            if body.startswith(('@@', 'diff --git ', '--- ')):
                break
            if body.startswith(' '):
                old += 1
                new += 1
            elif body.startswith('-'):
                old += 1
            elif body.startswith('+'):
                new += 1
        lines[i] = f'@@ -{match[1]},{old} +{match[2]},{new} @@{match[3]}\n'
    return ''.join(lines)

for name, spec in manifest['components'].items():
    if name == 'qtbase':
        continue
    work = store / f'{name}-repair'
    work.mkdir(exist_ok=False)
    with tarfile.open(store / f'{name}-{spec["version"]}.tar.xz') as tar:
        tar.extractall(work, filter='data')
    src = work / f'{name}-{spec["version"]}'
    run(['git', 'init', '-q'], src)
    run(['git', '-c', 'core.autocrlf=false', 'add', '.'], src)
    run(['git', '-c', 'user.name=Build baseline', '-c', 'user.email=build@localhost', 'commit', '-qm', 'Release baseline'], src)
    for patch in sorted((port / 'patches' / name).glob('*.patch')):
        temporary = store / 'normalized.patch'
        temporary.write_text(normalize(patch.read_text(encoding='utf-8')), encoding='utf-8', newline='\n')
        run([patch_exe, '-p1', '--batch', '--forward', '--fuzz=3', '-i', str(temporary)], src)
        # Stage new files too, so the generated diff includes their contents.
        run(['git', 'add', '-N', '.'], src)
        diff = subprocess.check_output(['git', 'diff', '--binary', '--', '.', ':!*.orig', ':!*.rej'], cwd=src)
        patch.write_bytes(diff)
        run(['git', 'add', '--', '.', ':!*.orig', ':!*.rej'], src)
        run(['git', '-c', 'user.name=Build baseline', '-c', 'user.email=build@localhost', 'commit', '-qm', patch.name], src)
    print(name, 'regenerated', flush=True)
