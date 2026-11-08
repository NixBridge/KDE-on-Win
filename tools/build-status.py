"""Print compact build progress without dumping compiler include traces."""
import re
import sys
sys.stdout.reconfigure(encoding="utf-8")
from pathlib import Path

root = Path(__file__).resolve().parents[1]
pipeline_path = next((p for p in [root / 'logs/build-port.log', root / 'work/build-port.log', root / 'build-port.log'] if p.exists()), None)
if pipeline_path is None:
    print('No build pipeline log found in logs/ or work/.')
    raise SystemExit(0)
pipeline = pipeline_path.read_text(encoding='utf-8', errors='replace')
packages = re.findall(r'^Building (.+)$', pipeline, re.M)
print('Current package:', packages[-1] if packages else 'initialization')
done = []
for path in sorted((root / 'logs').glob('*-build.log')):
    text = path.read_text(encoding='utf-8', errors='replace')
    if re.search(r'\*\*\* Craft all succeeded:|is up to date, nothing to do', text):
        done.append(path.name.removesuffix('-build.log'))
print('Compiled:', ', '.join(done))
if packages:
    current = packages[-1].strip()
    paths = [root / 'logs' / f'{current}-{phase}.log' for phase in ['deps', 'build']]
    paths = [p for p in paths if p.exists()]
    if paths:
        stages = re.findall(r'^Stage (deps|build) (.+)$', pipeline, re.M)
        latest = root / 'logs' / f'{current}-{stages[-1][0]}.log' if stages and stages[-1][1].strip() == current else max(paths, key=lambda p: p.stat().st_mtime)
        text = latest.read_text(encoding='utf-8', errors='replace')
        print('Log:', latest.name)
        progress = re.findall(r'\[\d+/\d+\] [^\r\n]+|\*\*\* (?:Handling package|Action):[^\r\n]+', text)
        if progress:
            print(progress[-1])
        errors = [line for line in text.splitlines() if re.search(r'CMake Error|error (?:C\d|LNK)|FAILED|fatal error:', line)]
        for line in errors[-3:]:
            print(line[:500])
