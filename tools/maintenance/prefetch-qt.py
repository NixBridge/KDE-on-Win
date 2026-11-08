"""Prefetch verified Qt binary archives while Craft installs build tools."""
import concurrent.futures
import hashlib
import json
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[2]
manifest = json.loads((root / 'work' / 'cache-26.05.json').read_text(encoding='utf-8'))
cache = Path('D:/KDE/CraftRoot/download/cache/26.05/windows/cl/msvc2022/x86_64/RelWithDebInfo')
base = 'https://files.kde.org/craft/Qt6/26.05/windows/cl/msvc2022/x86_64/RelWithDebInfo/'
names = {'qtbase', 'qtdeclarative', 'qttools', 'qtshadertools', 'qtsvg', 'qtlanguageserver', 'qttranslations', 'qtpositioning', 'qtlocation', 'qtwebsockets', 'qtmultimedia', 'qtimageformats'}
entries = []
for package in manifest['packages']['windows-cl-msvc2022-x86_64']:
    name = package['name'].rsplit('/', 1)[-1]
    if not package['name'].startswith('libs/qt6/') or name not in names:
        continue
    version = '6.11.1-2' if name == 'qtbase' else '6.11.1'
    build = next((b for b in package['build'] if b['version'] == version), None)
    if build:
        entries.append(build['files']['Binary'])

def fetch(entry):
    destination = cache / entry['fileName']
    if destination.exists():
        with destination.open('rb') as stream:
            if hashlib.file_digest(stream, 'sha256').hexdigest() == entry['checksum']:
                return str(destination)
        raise RuntimeError(f'Existing archive failed checksum: {destination}')
    destination.parent.mkdir(parents=True, exist_ok=True)
    partial = destination.with_suffix(destination.suffix + '.prefetch')
    subprocess.run(['curl.exe', '--noproxy', 'files.kde.org', '-fL', '--retry', '2', '--connect-timeout', '20', '-C', '-', '-o', str(partial), base + entry['fileName']], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    with partial.open('rb') as stream:
        assert hashlib.file_digest(stream, 'sha256').hexdigest() == entry['checksum'], destination
    partial.replace(destination)
    return str(destination)

with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
    for result in pool.map(fetch, entries):
        print(result, flush=True)
