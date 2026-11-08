"""Materialize the Windows port in a clean KDE Craft blueprint checkout."""
import argparse
import configparser
import json
import shutil
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--blueprints', type=Path, required=True)
parser.add_argument('--craft', type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
port = root / 'upstream/plasma-windows'
bp = args.blueprints.resolve()
prebuilt_versions = bp / 'libs/_msvc/version.ini'
text = prebuilt_versions.read_text(encoding='utf-8').replace('https://files.kde.org/craft/prebuilt/', 'https://mirror.twds.com.tw/kde-application/craft/prebuilt/')
prebuilt_versions.write_text(text, encoding='utf-8')
kde_pattern = bp / 'kde/kde.py'
text = kde_pattern.read_text(encoding='utf-8')
if '# Windows integration build policy' not in text:
    text = text.replace('        super().__init__(**kwargs)', '        super().__init__(**kwargs)\n        # Windows integration build policy\n        self.subinfo.options.configure.args += ["-DBUILD_TESTING=OFF"]', 1)
    kde_pattern.write_text(text, encoding='utf-8')
if args.craft:
    cmake_recipe = args.craft / 'blueprints/dev-utils/cmake-base/cmake-base.py'
    text = cmake_recipe.read_text(encoding='utf-8')
    if '"4.4.4"' not in text:
        text = text.replace('"4.3.3"]', '"4.3.3", "4.4.4"]')
    text = text.replace('self.defaultTarget = "4.3.3"', 'self.defaultTarget = "4.4.4"')
    cmake_recipe.write_text(text, encoding='utf-8')
    perl_recipe = args.craft / 'blueprints/dev-utils/perl/perl.py'
    text = perl_recipe.read_text(encoding='utf-8')
    if '"5.44.0"' not in text:
        text = text.replace('"5.40.1"]:', '"5.40.1", "5.44.0"]:')
    text = text.replace('self.defaultTarget = "5.40.1"', 'self.defaultTarget = "5.44.0"')
    perl_recipe.write_text(text, encoding='utf-8')
    python_recipe = args.craft / 'blueprints/libs/python/python.py'
    text = python_recipe.read_text(encoding='utf-8')
    if '"3.12.13"' not in text:
        text = text.replace('"3.14.6"]:', '"3.14.6", "3.12.13"]:')
    text = text.replace('self.defaultTarget = "3.14.6"', 'self.defaultTarget = "3.12.13"')
    python_recipe.write_text(text, encoding='utf-8')
    base_recipe = args.craft / 'blueprints/virtual/base/base.py'
    text = base_recipe.read_text(encoding='utf-8')
    text = '\n'.join(line for line in text.splitlines() if 'self.buildDependencies["craft/craft-' not in line)
    base_recipe.write_text(text + '\n', encoding='utf-8')
manifest = json.loads((port / 'upstream-versions.json').read_text())

globalaccel = bp / 'kde/frameworks/tier3/kglobalaccel/kglobalaccel.py'
text = globalaccel.read_text(encoding='utf-8')
text = '\n'.join(line for line in text.splitlines() if 'self.parent.package.categoryInfo.platforms =' not in line)
text = text.replace('def registerOptions(self):\n\n', 'def registerOptions(self):\n        pass\n\n')
if '# Windows client library' not in text:
    text = text.replace('        super().__init__(**kwargs)', '        super().__init__(**kwargs)\n        # Windows client library\n        self.subinfo.options.configure.args += ["-DKF_IGNORE_PLATFORM_CHECK=ON"]')
text = text.replace('["-DKF_IGNORE_PLATFORM_CHECK=ON"]', '["-DKF_IGNORE_PLATFORM_CHECK=ON", "-DWITH_X11=OFF"]')
globalaccel.write_text(text + '\n', encoding='utf-8')

def save(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding='utf-8', newline='\n')

for category, version in [('kde/frameworks', '6.28.0'), ('kde/plasma', '6.7.4')]:
    ini = bp / category / 'version.ini'
    cfg = configparser.ConfigParser(interpolation=None)
    cfg.read(ini, encoding='utf-8')
    versions = cfg['General'].get('tarballs', '').split(';')
    cfg['General']['tarballs'] = ';'.join(dict.fromkeys(versions + [version]))
    cfg['General']['defaulttarget'] = version
    if category == 'kde/plasma':
        for key in ('tarballurl', 'tarballdigesturl'):
            if key in cfg['General']:
                cfg['General'][key] = cfg['General'][key].replace('https://download.kde.org/stable/plasma/', 'https://mirror.twds.com.tw/kde/stable/plasma/')
    with ini.open('w', encoding='utf-8', newline='\n') as stream:
        cfg.write(stream)

qt_ini = bp / 'libs/qt6/version.ini'
qt_text = qt_ini.read_text(encoding='utf-8')
import re
save(qt_ini, re.sub(r'(?m)^defaulttarget\s*=.*$', 'defaulttarget = 6.11.1', qt_text))

# Some required shell packages have no upstream Craft blueprint.
for name, deps in {
    'kactivitymanagerd': ['kde/plasma/plasma-activities', 'kde/frameworks/tier3/kio', 'kde/frameworks/tier2/kcrash', 'libs/qt/qtsql'],
    'plasma-desktop': ['kde/plasma/plasma-workspace', 'kde/plasma/plasma5support'],
    'plasma5support': ['kde/plasma/libplasma', 'libs/qt/qtdeclarative'],
    'knighttime': ['libs/qt/qtpositioning', 'kde/frameworks/tier1/kholidays', 'kde/frameworks/tier1/kdbusaddons', 'kde/frameworks/tier1/kconfig'],
}.items():
    target = bp / 'kde/plasma' / name / f'{name}.py'
    if not target.exists():
        # QtSql is part of qtbase, not a separate Qt module.
        deps = ['libs/qt/qtbase' if dep.endswith('/qtsql') else dep for dep in deps]
        lines = '\n'.join(f'        self.runtimeDependencies[{dep!r}] = None' for dep in deps)
        save(target, f'''import info
from Blueprints.CraftPackageObject import CraftPackageObject

class subinfo(info.infoclass):
    def setTargets(self):
        self.versionInfo.setDefaultValues()
    def setDependencies(self):
        self.buildDependencies["virtual/base"] = None
        self.buildDependencies["kde/frameworks/extra-cmake-modules"] = None
{lines}

class Package(CraftPackageObject.get("kde/plasma").pattern):
    def __init__(self, **kwargs):
        super().__init__(**kwargs)
''')

for patch_dir in sorted((port / 'patches').iterdir()):
    if not patch_dir.is_dir() or patch_dir.name == 'qtbase':
        continue  # SDK >= 22621 does not require the Qt 19041 fallback.
    name = patch_dir.name
    candidates = list((bp / 'kde').rglob(f'{name}.py'))
    if len(candidates) != 1:
        raise RuntimeError(f'{name}: expected one blueprint, got {candidates}')
    target = candidates[0]
    text = target.read_text(encoding='utf-8')
    for patch in sorted(patch_dir.glob('*.patch')):
        shutil.copyfile(patch, target.parent / patch.name)
    if '# Windows port overlay' in text:
        version = manifest['components'].get(name, manifest.get('git_components', {}).get(name, {})).get('version', '6.28.0')
        patches = sorted(patch_dir.glob('*.patch'))
        text = re.sub(r'(?m)^        self\.patchToApply\.setdefault.*$', f'        self.patchToApply.setdefault({version!r}, []).extend({[(p.name, 1) for p in patches]!r})', text)
        save(target, text)
        continue
    # Enable only the packages actually patched by this port.
    text = '\n'.join(line for line in text.splitlines() if 'self.parent.package.categoryInfo.platforms =' not in line)
    text = text.replace('def registerOptions(self):\n\n', 'def registerOptions(self):\n        pass\n\n')
    if name == 'plasma-workspace':
        text = '\n'.join(line for line in text.splitlines() if 'self.runtimeDependencies["kde/plasma/kwin"]' not in line)
    version = manifest['components'].get(name, manifest.get('git_components', {}).get(name, {})).get('version', '6.28.0')
    patches = sorted(patch_dir.glob('*.patch'))
    for patch in patches:
        shutil.copyfile(patch, target.parent / patch.name)
    addition = '\n        # Windows port overlay\n'
    addition += f'        self.patchToApply.setdefault({version!r}, []).extend({[(p.name, 1) for p in patches]!r})\n'
    marker = '        self.versionInfo.setDefaultValues()'
    if marker not in text:
        raise RuntimeError(f'{name}: unknown blueprint structure')
    text = text.replace(marker, marker + addition, 1)
    marker = '        super().__init__(**kwargs)'
    text = text.replace(marker, marker + '\n        self.subinfo.options.configure.args += ["-DKF_IGNORE_PLATFORM_CHECK=ON", "-DWITH_X11=OFF", "-DWITH_WAYLAND=OFF", "-DBUILD_TESTING=OFF", "-DBUILD_KAMD_SQLITE_PLUGIN=OFF"]', 1)
    compile(text, str(target), 'exec')
    save(target, text + '\n')

workspace = bp / 'kde/plasma/plasma-workspace/plasma-workspace.py'
text = workspace.read_text(encoding='utf-8')
if '# Notification chart runtime' not in text:
    text = text.replace('    def setDependencies(self):', '    def setDependencies(self):\n        # Notification chart runtime\n        self.runtimeDependencies["kde/frameworks/tier1/kquickcharts"] = None', 1)
    save(workspace, text)
kio = bp / 'kde/frameworks/tier3/kio/kio.py'
text = kio.read_text(encoding='utf-8')
if '# Native session D-Bus support' not in text:
    text = text.replace('        self.versionInfo.setDefaultValues()', '        self.versionInfo.setDefaultValues()\n        # Native session D-Bus support\n        self.patchLevel["6.28.0"] = 1', 1)
    text = text.replace('        super().__init__(**kwargs)', '        super().__init__(**kwargs)\n        self.subinfo.options.configure.args += ["-DUSE_DBUS=ON"]', 1)
    save(kio, text)
activity_service = bp / 'kde/plasma/kactivitymanagerd/kactivitymanagerd.py'
text = activity_service.read_text(encoding='utf-8').replace('-DBUILD_KAMD_SQLITE_PLUGIN=OFF', '-DBUILD_KAMD_SQLITE_PLUGIN=ON')
if '# Activity database support' not in text:
    text = text.replace('        self.versionInfo.setDefaultValues()', '        self.versionInfo.setDefaultValues()\n        # Activity database support\n        self.patchLevel["6.7.4"] = 1', 1)
save(activity_service, text)
extras = bp / 'kde/kdenetwork/kio-extras/kio-extras.py'
text = extras.read_text(encoding='utf-8')
if '-DUSE_DBUS=ON' not in text:
    text = text.replace('        super().__init__(**kwargs)', '        super().__init__(**kwargs)\n        self.subinfo.options.configure.args += ["-DUSE_DBUS=ON"]', 1)
    save(extras, text)
nighttime = bp / 'kde/plasma/knighttime/knighttime.py'
text = nighttime.read_text(encoding='utf-8')
if '# Verified SHA256 release input' not in text:
    records = json.loads((root / 'manifests' / 'extra-source-verification.json').read_text(encoding='utf-8'))
    digest = next(record['sha256'] for record in records if record['name'] == 'knighttime')
    text = 'from Utils import CraftHash\n' + text
    addition = f'\n        # Verified SHA256 release input\n        self.targetDigestUrls.pop("6.7.4", None)\n        self.targetDigests["6.7.4"] = ([{digest!r}], CraftHash.HashAlgorithm.SHA256)'
    save(nighttime, text.replace('        self.versionInfo.setDefaultValues()', '        self.versionInfo.setDefaultValues()' + addition))
support = bp / 'kde/plasma/plasma5support/plasma5support.py'
text = support.read_text(encoding='utf-8')
if '# Support library framework dependencies' not in text:
    deps = ['kde/frameworks/tier1/kidletime', 'kde/frameworks/tier2/kunitconversion', 'kde/frameworks/tier1/kholidays']
    addition = '\n        # Support library framework dependencies\n' + '\n'.join(f'        self.runtimeDependencies[{dep!r}] = None' for dep in deps)
    save(support, text.replace('    def setDependencies(self):', '    def setDependencies(self):' + addition))
libplasma = bp / 'kde/plasma/libplasma/libplasma.py'
text = libplasma.read_text(encoding='utf-8')
if 'self.runtimeDependencies["libs/qt6/qt5compat"]' not in text:
    text = text.replace('    def setDependencies(self):', '    def setDependencies(self):\n        self.runtimeDependencies["libs/qt6/qt5compat"] = None')
    save(libplasma, text)
text = workspace.read_text(encoding='utf-8').replace('kde/frameworks/tier2/kholidays', 'kde/frameworks/tier1/kholidays')
save(workspace, text)
if '# Additional shell build dependencies' not in text:
    deps = ['qt-libs/qcoro', 'libs/qt/qtlocation', 'libs/qt/qtpositioning', 'libs/qt/qtshadertools', 'kde/plasma/knighttime', 'kde/frameworks/tier3/kded', 'kde/frameworks/tier3/ktexteditor', 'kde/frameworks/tier2/kholidays', 'kde/frameworks/tier1/kstatusnotifieritem']
    addition = '\n        # Additional shell build dependencies\n' + '\n'.join(f'        self.runtimeDependencies[{dep!r}] = None' for dep in deps)
    text = text.replace('    def setDependencies(self):', '    def setDependencies(self):' + addition)
    save(workspace, text)

text = workspace.read_text(encoding='utf-8')
if '# Shell QML runtime assets' not in text:
    deps = ['libs/qt6/qt5compat', 'kde/plasma/breeze', 'kde/frameworks/tier1/breeze-icons', 'kde/unreleased/kirigami-addons']
    addition = '\n        # Shell QML runtime assets\n' + '\n'.join(f'        self.runtimeDependencies[{dep!r}] = None' for dep in deps)
    text = text.replace('    def setDependencies(self):', '    def setDependencies(self):' + addition)
    save(workspace, text)

for name, spec in manifest['components'].items():
    if name == 'qtbase':
        continue
    matches = list((bp / 'kde').rglob(f'{name}.py'))
    if len(matches) != 1:
        raise RuntimeError(f'{name}: cannot pin release digest')
    target = matches[0]
    text = target.read_text(encoding='utf-8')
    if '# Pinned release archive SHA256' not in text:
        if 'from Utils import CraftHash' not in text:
            text = 'from Utils import CraftHash\n' + text
        addition = f'\n        # Pinned release archive SHA256\n        self.targetDigestUrls.pop({spec["version"]!r}, None)\n        self.targetDigests[{spec["version"]!r}] = ([{spec["sha256"]!r}], CraftHash.HashAlgorithm.SHA256)'
        text = text.replace('        self.versionInfo.setDefaultValues()', '        self.versionInfo.setDefaultValues()' + addition, 1)
        save(target, text)

print('Windows blueprint overlay configured:', bp)

# Resolve framework tiers from this pinned checkout rather than assuming tiers.
for target in (bp / 'kde/plasma').rglob('*.py'):
    text = target.read_text(encoding='utf-8')
    if '-DWITH_X11_SESSION=OFF' not in text:
        text = text.replace('        super().__init__(**kwargs)', '        super().__init__(**kwargs)\n        self.subinfo.options.configure.args += ["-DWITH_X11_SESSION=OFF", "-DWITHOUT_X11=ON"]', 1)
    for dependency in set(re.findall(r'libs/qt/[a-z0-9-]+', text)):
        leaf = dependency.rsplit('/', 1)[-1]
        if not (bp / dependency / f'{leaf}.py').exists() and (bp / 'libs/qt6' / leaf / f'{leaf}.py').exists():
            text = text.replace(dependency, f'libs/qt6/{leaf}')
    for dependency in set(re.findall(r'kde/frameworks/tier\d+/[a-z0-9-]+', text)):
        leaf = dependency.rsplit('/', 1)[-1]
        matches = list((bp / 'kde/frameworks').rglob(f'{leaf}.py'))
        if len(matches) == 1:
            text = text.replace(dependency, matches[0].parent.relative_to(bp).as_posix())
    compile(text, str(target), 'exec')
    save(target, text)
