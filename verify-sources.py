"""Fetch pinned upstream sources, verify digests, and apply port patches."""
import concurrent.futures
import argparse
import hashlib
import json
import subprocess
import tarfile
from pathlib import Path

root = Path(__file__).resolve().parent
port = root / 'upstream/plasma-windows'
store = Path('D:/KDE/sources')
store.mkdir(parents=True, exist_ok=True)
parser = argparse.ArgumentParser()
parser.add_argument('--suffix', default='work')
args = parser.parse_args()
manifest = json.loads((port / 'upstream-versions.json').read_text())

def verify(item):
    name, spec = item
    version = spec['version']
    filename = f'{name}-{version}.tar.xz'
    family = f'plasma/{version}' if version.startswith('6.7.') else 'frameworks/6.28'
    archive = store / filename
    url = f'https://download.kde.org/stable/{family}/{filename}'
    subprocess.run(['curl.exe', '-fL', '--retry', '2', '--connect-timeout', '20', '--max-time', '240', '-C', '-', '-o', str(archive), url], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    digest = hashlib.file_digest(archive.open('rb'), 'sha256').hexdigest()
    if digest != spec['sha256']:
        raise RuntimeError(f'{name}: SHA256 mismatch: {digest}')
    work = store / f'{name}-{args.suffix}'
    if work.exists():
        raise RuntimeError(f'{work} already exists; refusing to overwrite verification input')
    work.mkdir()
    with tarfile.open(archive) as tar:
        tar.extractall(work, filter='data')
    source = work / f'{name}-{version}'
    log = []
    for patch in sorted((port / 'patches' / name).glob('*.patch')):
        result = subprocess.run(['git', 'apply', '--verbose', str(patch)], cwd=source, capture_output=True, text=True)
        log.append(f'{patch.name}: {result.returncode}\n{result.stdout}{result.stderr}')
        if result.returncode:
            (store / f'{name}-patch.log').write_text('\n'.join(log), encoding='utf-8')
            raise RuntimeError(f'{name}: patch failed: {patch.name}')
    (store / f'{name}-patch.log').write_text('\n'.join(log), encoding='utf-8')
    return {'name': name, 'url': url, 'sha256': digest, 'patches_applied': len(log)}

results = []
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
    futures = {pool.submit(verify, item): item[0] for item in manifest['components'].items() if item[0] != 'qtbase'}
    for future in concurrent.futures.as_completed(futures):
        name = futures[future]
        try:
            result = future.result()
        except Exception as exc:
            result = {'name': name, 'error': str(exc)}
        results.append(result)
        print(json.dumps(result), flush=True)
(root / 'source-verification.json').write_text(json.dumps(results, indent=2), encoding='utf-8')
raise SystemExit(1 if any('error' in result for result in results) else 0)
