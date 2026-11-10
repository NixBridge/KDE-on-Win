# SPDX-License-Identifier: GPL-3.0-or-later
"""Check packaged relative service directories on a private ephemeral D-Bus."""
import os
import queue
from pathlib import Path
import subprocess
import tempfile
import threading

root = Path(__file__).resolve().parents[1]
runtime = Path(os.environ.get('KDE_CRAFT_ROOT', 'D:/KDE/CraftRoot'))
with tempfile.TemporaryDirectory(prefix='dbus-relocation-', dir=root / 'work') as folder:
    folder = Path(folder) / '中文 space & (directory)!'
    services = folder / 'bin/data/dbus-1/services'
    services.mkdir(parents=True)
    (services / 'org.nixbridge.RelocationProbe.service').write_text(
        '[D-BUS Service]\nName=org.nixbridge.RelocationProbe\nExec=never-executed-fixture\n', encoding='utf-8')
    text = (root / 'upstream/plasma-windows/tools/dbus/session-plasma.conf').read_text(encoding='utf-8')
    text = text.replace('port=12443', 'port=0').replace('<standard_session_servicedirs />', '')
    text = text.replace('<auth>DBUS_COOKIE_SHA1</auth>', '<auth>ANONYMOUS</auth><allow_anonymous/>')
    config = folder / 'session.conf'
    config.write_text(text, encoding='utf-8')
    daemon = subprocess.Popen([str(runtime / 'bin/dbus-daemon.exe'), '--nofork', '--print-address=1',
                               '--config-file=' + str(config)], stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                              cwd=root)
    try:
        addresses = queue.Queue()
        reader = threading.Thread(target=lambda: addresses.put(daemon.stdout.readline()), daemon=True)
        reader.start()
        address = addresses.get(timeout=10).decode().strip()
        assert address.startswith('tcp:'), address
        result = subprocess.run([str(runtime / 'bin/dbus-send.exe'), '--bus=' + address,
                                 '--print-reply', '--dest=org.freedesktop.DBus', '/org/freedesktop/DBus',
                                 'org.freedesktop.DBus.ListActivatableNames'], capture_output=True, timeout=10)
        assert result.returncode == 0, (result.stdout, result.stderr)
        assert b'org.nixbridge.RelocationProbe' in result.stdout, result.stdout
    finally:
        daemon.terminate()
        daemon.communicate(timeout=10)
print('PASS: relative service discovery from relocated Unicode config, independent of cwd; private bus only, no services activated.')
