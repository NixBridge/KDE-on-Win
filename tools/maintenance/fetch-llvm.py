"""Resume the LLVM archive with verified HTTP range downloads."""
import concurrent.futures
import hashlib
import json
import shutil
import subprocess
import urllib.request
from pathlib import Path

base = 'https://mirror.twds.com.tw/kde-application/craft/Qt6/26.05/windows/cl/msvc2022/x86_64/Release/'
manifest = json.load(urllib.request.urlopen(base + 'manifest.json', timeout=30))
package = next(p for p in manifest['packages']['windows-cl-msvc2022-x86_64'] if p['name'] == 'libs/llvm')
build = next(b for b in package['build'] if b['version'] == '20.1.7')
entry = build['files']['Binary']
url = base + entry['fileName']
destination = Path('D:/KDE/CraftRoot/download/cache/26.05/windows/cl/msvc2022/x86_64/Release') / entry['fileName']
total = int(urllib.request.urlopen(urllib.request.Request(url, method='HEAD'), timeout=30).headers['Content-Length'])
prefix = destination.stat().st_size if destination.exists() else 0
assert prefix <= total
parts = []
step = 4 * 1024 * 1024
for i, start in enumerate(range(prefix, total, max(1, step))):
    end = min(start + step, total) - 1
    parts.append((destination.with_suffix(f'.range-{prefix}-{i}'), start, end))

def fetch(part):
    path, start, end = part
    if path.exists() and path.stat().st_size == end - start + 1:
        return
    result = subprocess.run(['curl.exe', '--noproxy', 'mirror.twds.com.tw', '-fsSL', '--retry', '3', '--connect-timeout', '20', '--max-time', '90', '--range', f'{start}-{end}', '-o', str(path), '-w', '%{http_code}', url], capture_output=True, text=True, check=True)
    assert result.stdout.strip() == '206', result.stdout
    assert path.stat().st_size == end - start + 1
    print('Range complete:', start, end, flush=True)

with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:
    list(pool.map(fetch, parts))
assembled = destination.with_suffix('.assembled')
with assembled.open('wb') as out:
    if prefix:
        with destination.open('rb') as stream:
            shutil.copyfileobj(stream, out)
    for path, _, _ in parts:
        with path.open('rb') as stream:
            shutil.copyfileobj(stream, out)
with assembled.open('rb') as stream:
    digest = hashlib.file_digest(stream, 'sha256').hexdigest()
assert digest == entry['checksum'], digest
assembled.replace(destination)
print('LLVM SHA256 verified:', digest, flush=True)
