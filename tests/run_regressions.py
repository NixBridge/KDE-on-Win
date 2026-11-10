# SPDX-License-Identifier: GPL-3.0-or-later
"""Isolated Windows integration regression suite; never starts a live desktop or installs hooks."""
import argparse
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path
root = Path(__file__).resolve().parents[1]
os.chdir(root)
(root / 'work').mkdir(exist_ok=True)
env = os.environ.copy()
parser = argparse.ArgumentParser()
parser.add_argument('--runtime', type=Path, default=Path(os.environ.get('KDE_CRAFT_ROOT', 'D:/KDE/CraftRoot')))
parser.add_argument('--results', type=Path, default=root / 'work/fix22-test-results.json')
options = parser.parse_args()
options.runtime = options.runtime.resolve()
env['KDE_CRAFT_ROOT'] = str(options.runtime)
env.update(QT_QPA_PLATFORM='offscreen', QT_PLUGIN_PATH=str(options.runtime / 'plugins'), QML_IMPORT_PATH=str(options.runtime / 'qml'), QML2_IMPORT_PATH=str(options.runtime / 'qml'), QT_QUICK_BACKEND='software', DBUS_SESSION_BUS_ADDRESS='tcp:host=127.0.0.1,port=19999')
checks = [('plasma-desktop-tools', ['--self-test']),
 ('plasma-desktop-tools', ['--ui-smoke-test']),
 ('plasma-desktop-tools', ['--ui-smoke-test','--expect-chinese']),
 ('input-layout-probe', []),
 ('plasma-system-settings', ['--ui-smoke-test','--page=display']),
 ('plasma-system-settings', ['--ui-smoke-test','--page=appearance']),
 ('plasma-system-settings', ['--ui-smoke-test','--page=invalid']),
 ('kde-session-host', ['--self-test']),
 ('trayhost', ['--self-test']),
 ('shellswitch', ['--self-test']),
 ('volume-lifecycle-probe', []),
 ('notification-callback-probe', []),
 ('notification-relay-probe', []),
 ('kde-style-host', ['--self-test']),
 ('x86/kde-style-host', ['--self-test']),
 ('kde-logon-probe', []),
 ('breeze-window-probe', ['--dialog-self-test']),
 ('plasma-open-with', ['--ui-smoke-test']),
 ('plasma-open-with', ['--ui-smoke-test', '--expect-chinese']),
 ('plasma-open-with', ['--invoke-self-test']),
 ('konsole-conpty-probe', []),
 ('konsole-session-probe', []),
 ('plasma-system-settings', ['--ui-smoke-test']),
 ('plasma-system-settings', ['--ui-smoke-test', '--expect-chinese']),
 ('plasma-text-editor', ['--ui-smoke-test']),
 ('plasma-text-editor', ['--self-test']),
 ('plasma-text-editor', ['--editing-self-test']),
 ('plasma-text-editor', ['--ui-smoke-test', '--expect-chinese']),
 ('plasma-process-monitor', ['--ui-smoke-test']),
 ('plasma-process-monitor', ['--self-test']),
 ('plasma-archive-manager', ['--ui-smoke-test']),
 ('breeze-window-probe', []),
 ('breeze-window-probe', ['--caption-contrast-self-test']),
 ('plasma-window-decorations', ['--self-test']),
 ('appearance-sync-probe', []),
 ('personalization-probe', []),
 ('taskbar-icon-probe', []),
 ('startmenu-model-probe', []),
 ('qml-import-probe', ['--check-startmenu-model']),
 ('qml-import-probe', []),
 ('plasma-notification-bridge', ['--check-platform'])]
results = []
with tempfile.TemporaryDirectory(prefix='fix22-validation-', dir=root / 'work', delete=False) as folder:
    for index, (name, args) in enumerate(checks):
        directory = Path(folder) / str(index)
        directory.mkdir()
        prefs = directory / 'appearance.ini'
        prefs.write_text('[General]\nlanguage=' + ('zh_CN' if '--expect-chinese' in args else 'en_US') + '\n', encoding='utf-8')
        current = env.copy()
        current['KDE_ON_WIN_SETTINGS'] = str(prefs)
        p = subprocess.run([str(options.runtime / 'bin' / (name + '.exe')), *args], env=current, cwd=directory, capture_output=True, timeout=40)
        item = {'name': name + ' ' + ' '.join(args), 'exit': p.returncode, 'stdout': p.stdout.decode('utf-8', errors='replace').strip(), 'stderr': p.stderr.decode('utf-8', errors='replace').strip()}
        results.append(item)
        print(item['name'], p.returncode, item['stdout'][-1000:], flush=True)
    for name, args in [('window-actions-probe', []), ('desktop-stacking-probe', []), ('breeze-window-probe', ['--native-frame-self-test']), ('plasma-window-decorations', ['--self-test'])]:
        current = env.copy()
        current['QT_QPA_PLATFORM'] = 'windows'
        current['KDE_ON_WIN_SETTINGS'] = str(prefs)
        p = subprocess.run([str(options.runtime / 'bin' / (name + '.exe')), *args], env=current, capture_output=True, timeout=20)
        results.append({'name': 'native ' + name, 'exit': p.returncode, 'stdout': p.stdout.decode('utf-8', errors='replace').strip(), 'stderr': p.stderr.decode('utf-8', errors='replace').strip()})
        print(results[-1]['name'], p.returncode, results[-1]['stdout'], flush=True)
for command in [['python', 'tests/test_archive_worker.py'],
 ['node', 'tests/taskbar-migration.cjs'],
 ['python', 'tests/test_association_plan.py'],
 ['python', 'tests/test_folder_routing.py'],
 ['python', 'tests/test_logon_plan.py'],
 ['python', 'tests/test_logon_deployment.py'],
 ['python', 'tests/test_integration_binaries.py'],
 ['python', 'tests/test_translation_catalog.py'],
 ['python', 'tests/test_settings_scripts.py'],
 ['python', 'tests/test_session_log_fallback.py'],
 ['python', 'tests/test_cumulative_contract.py'],
 ['python', 'tests/test_runtime_relocation.py'],
 ['python', 'tests/test_dbus_relocation.py'],
 ['python', 'tests/test_msvc_deps.py']]:
    if command[0] == 'python':
        command[0] = sys.executable
    p = subprocess.run(command, env=env, capture_output=True, timeout=40)
    results.append({'name': command[1], 'exit': p.returncode, 'stdout': p.stdout.decode('utf-8', errors='replace').strip(), 'stderr': p.stderr.decode('utf-8', errors='replace').strip()})
    print(command[1], p.returncode, results[-1]['stdout'], flush=True)
options.results.write_text(json.dumps(results, indent=2) + '\n', encoding='utf-8')
raise SystemExit(any((r['exit'] for r in results)))
