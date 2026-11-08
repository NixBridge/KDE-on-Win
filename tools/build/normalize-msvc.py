# SPDX-License-Identifier: GPL-3.0-or-later
"""Normalize localized MSVC dependency output for Ninja, without changing diagnostics."""
import subprocess
import sys

PREFIXES = ('Note: including file:', '注意: 包含文件:')


def normalized_line(data):
    try:
        text = data.decode('utf-8')
    except UnicodeDecodeError:
        # MSVC's Chinese language pack emits the current ANSI code page.
        text = data.decode('mbcs' if sys.platform == 'win32' else 'cp936', errors='replace')
    for prefix in PREFIXES:
        if text.startswith(prefix):
            return ('Note: including file: ' + text[len(prefix):].lstrip()).encode('utf-8')
    return text.encode('utf-8')


def main():
    process = subprocess.Popen(sys.argv[1:], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    for line in iter(process.stdout.readline, b''):
        sys.stdout.buffer.write(normalized_line(line))
        sys.stdout.buffer.flush()
    return process.wait()


if __name__ == '__main__':
    raise SystemExit(main())
