# SPDX-License-Identifier: GPL-3.0-or-later
"""Assemble a desktop distribution from the base archive and a cumulative patch."""
import argparse
import hashlib
import shutil
import zipfile
from pathlib import Path, PurePosixPath

TOOLCHAIN_PROGRAMS = {
    'bugpoint.exe', 'c-index-test.exe', 'dsymutil.exe', 'find-all-symbols.exe',
    'llc.exe', 'lld.exe', 'lld-link.exe', 'ld.lld.exe', 'lli.exe',
    'lli-child-target.exe', 'modularize.exe', 'opt.exe', 'pp-trace.exe', 'wasm-ld.exe',
}


def build_tool(name):
    path = PurePosixPath(name)
    return path.parent == PurePosixPath('bin') and path.suffix == '.exe' and (
        path.name in TOOLCHAIN_PROGRAMS
        or path.stem in {'clang', 'clangd', 'clang++', 'clang-cl', 'clang-cpp'}
        or path.name.startswith(('clang-', 'llvm-'))
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base', required=True, type=Path)
    parser.add_argument('--overlay', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    with zipfile.ZipFile(args.base) as base, zipfile.ZipFile(args.overlay) as overlay:
        overlay_names = set(overlay.namelist())
        skipped = [item for item in base.infolist() if build_tool(item.filename)
                   and item.filename not in overlay_names]
        excluded = {item.filename for item in skipped}
        with zipfile.ZipFile(args.output, 'x', compression=zipfile.ZIP_DEFLATED) as out:
            for archive in (base, overlay):
                for entry in archive.infolist():
                    if archive is base and (entry.filename in overlay_names or entry.filename in excluded):
                        continue
                    target = zipfile.ZipInfo(entry.filename, entry.date_time)
                    target.external_attr = entry.external_attr
                    target.compress_type = zipfile.ZIP_DEFLATED
                    with archive.open(entry) as source, out.open(target, 'w') as destination:
                        shutil.copyfileobj(source, destination, length=1024 * 1024)
    with zipfile.ZipFile(args.output) as archive:
        broken = archive.testzip()
        if broken:
            raise SystemExit(f'ZIP CRC mismatch: {broken}')
    with args.output.open('rb') as file:
        digest = hashlib.file_digest(file, 'sha256').hexdigest()
    args.output.with_suffix('.zip.sha256').write_text(f'{digest} *{args.output.name}\n', encoding='ascii')
    print(f'{args.output}: {args.output.stat().st_size:,} bytes, excluded {len(excluded)} build tools')


if __name__ == '__main__':
    main()
