# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise real archive workers without changing file associations."""
import argparse
import io
import json
import subprocess
import tarfile
import tempfile
import zipfile
from pathlib import Path

parser=argparse.ArgumentParser()
parser.add_argument('--exe',type=Path,default=Path('D:/KDE/CraftRoot/bin/plasma-archive-manager.exe'))
args=parser.parse_args()
workspace=Path(__file__).resolve().parents[1]

def run(*arguments):
    p=subprocess.run([str(args.exe),*map(str,arguments)],capture_output=True,timeout=30)
    data=json.loads(p.stdout.decode('utf-8').strip())
    return p.returncode,data

with tempfile.TemporaryDirectory(prefix='archive-regression-',dir=workspace/'work') as temporary:
    root=Path(temporary).resolve()
    assert root.is_relative_to(workspace.resolve())
    destination=root/'output';destination.mkdir()
    content='中文文件内容\n'.encode()
    valid=root/'normal.zip'
    with zipfile.ZipFile(valid,'w',zipfile.ZIP_DEFLATED) as z:
        z.writestr('folder/测试.txt',content)
        z.writestr('empty.txt',b'')
    code,data=run('--list',valid)
    assert code==0 and any(x['path']=='folder/测试.txt' for x in data['entries']),data
    code,data=run('--extract',valid,destination)
    assert code==0,data
    extracted=Path(data['destination'])
    assert extracted.is_relative_to(destination)
    assert (extracted/'folder/测试.txt').read_bytes()==content
    assert (extracted/'empty.txt').read_bytes()==b''
    for index,name in enumerate(['../outside.txt','/absolute.txt','C:/escape.txt','folder/name:stream','CON.txt','folder/../escape.txt','trailing.','back\\slash.txt']):
        bad=root/f'bad-{index}.zip'
        with zipfile.ZipFile(bad,'w') as z:z.writestr(name.replace('\\','/'),b'escape')
        if '\\' in name:bad.write_bytes(bad.read_bytes().replace(name.replace('\\','/').encode(),name.encode()))
        code,data=run('--extract',bad,destination)
        assert code!=0 and data['error'],(name,data)
        assert not (root/'outside.txt').exists()
    seven=root/'normal.7z'
    sevenzip=Path('D:/KDE/CraftRoot/dev-utils/bin/7za.exe')
    if sevenzip.exists():
        source=root/'seven-source';source.mkdir();(source/'\u6d4b\u8bd5.txt').write_bytes(content)
        subprocess.run([str(sevenzip),'a',str(seven),'\u6d4b\u8bd5.txt'],cwd=source,check=True,capture_output=True)
        code,data=run('--extract',seven,destination)
        assert code==0,data
        assert (Path(data['destination'])/'\u6d4b\u8bd5.txt').read_bytes()==content
    link=root/'link.tar' 
    with tarfile.open(link,'w') as tar:
        info=tarfile.TarInfo('link');info.type=tarfile.SYMTYPE;info.linkname='../outside';tar.addfile(info)
    code,data=run('--extract',link,destination)
    assert code!=0 and data['error'],data
    pax=root/'pax.tar'
    with tarfile.open(pax,'w',format=tarfile.PAX_FORMAT) as tar:
        info=tarfile.TarInfo('pax-name.txt');info.pax_headers={'path':'actual-name.txt'};info.size=3;tar.addfile(info,io.BytesIO(b'abc'))
    code,data=run('--extract',pax,destination)
    assert code!=0 and data['error'],data
    gnu=root/'gnu.tar'
    with tarfile.open(gnu,'w',format=tarfile.GNU_FORMAT) as tar:
        info=tarfile.TarInfo('\u76ee\u5f55/\u6d4b\u8bd5.txt');info.size=len(content);tar.addfile(info,io.BytesIO(content))
    code,data=run('--extract',gnu,destination)
    assert code==0,data
    assert (Path(data['destination'])/'\u76ee\u5f55/\u6d4b\u8bd5.txt').read_bytes()==content
    for suffix,mode in [('tar','w'),('tar.gz','w:gz'),('tar.xz','w:xz')]:
        archive=root/f'test.{suffix}'
        with tarfile.open(archive,mode) as tar:
            info=tarfile.TarInfo('test.txt');info.size=len(content);tar.addfile(info,io.BytesIO(content))
        code,data=run('--extract',archive,destination)
        assert code==0,data
        assert (Path(data['destination'])/'test.txt').read_bytes()==content
print('PASS: ZIP and compressed TAR listing/extraction, Unicode, empty files; traversal, absolute paths, ADS, reserved names, trailing dots and symlinks rejected.')
