# SPDX-License-Identifier: GPL-3.0-or-later
"""Stamp the packaged source revision, never guess the final commit at compile time."""
import argparse,json,os,re,subprocess,tempfile
from pathlib import Path
parser=argparse.ArgumentParser()
parser.add_argument('--runtime',type=Path,required=True)
parser.add_argument('--version',required=True)
args=parser.parse_args()
if not re.fullmatch(r'Fix[0-9]+|development',args.version):parser.error('invalid release version')
root=Path(__file__).resolve().parents[1]
if not (args.runtime/'bin').is_dir():parser.error('runtime bin directory is missing')
commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip()
if not re.fullmatch('[0-9a-f]{40}',commit):raise SystemExit('invalid Git revision')
dirty=bool(subprocess.check_output(['git','status','--porcelain','--untracked-files=normal'],cwd=root,text=True).strip())
target=args.runtime/'bin/data/kde-on-win/build-info.json';target.parent.mkdir(parents=True,exist_ok=True)
with tempfile.NamedTemporaryFile(mode='w',encoding='utf-8',newline='\n',dir=target.parent,delete=False) as file:
    temporary=Path(file.name)
    json.dump({'version':args.version,'commit':commit,'dirty':dirty},file,indent=2);file.write('\n')
try:os.replace(temporary,target)
finally:temporary.unlink(missing_ok=True)
