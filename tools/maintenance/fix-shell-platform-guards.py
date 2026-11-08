"""Regenerate the shell platform-boundary patch from the unpacked source."""
import re
import subprocess
from pathlib import Path

source = Path('D:/KDE/CraftRoot/build/kde/plasma/plasma-workspace/work/plasma-workspace-6.7.4')
baseline = Path('D:/KDE/sources/plasma-workspace-repair/plasma-workspace-6.7.4')
names = ['desktopview.cpp', 'panelview.cpp', 'panelconfigview.cpp', 'shellcorona.cpp']

def lexical(text):
    pattern = r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\x27(?:\\.|[^\x27\\])*\x27'
    return re.sub(pattern, lambda m: ''.join('\n' if c == '\n' else ' ' for c in m[0]), text, flags=re.S)

def closing(code, start, left, right):
    depth = 0
    for index in range(start, len(code)):
        if code[index] == left:
            depth += 1
        elif code[index] == right:
            depth -= 1
            if depth == 0:
                return index
    raise RuntimeError('Unbalanced source delimiter')

def guard_function(text, signature):
    code = lexical(text)
    start = code.index('{', code.index(signature))
    end = closing(code, start, '{', '}')
    return text[:start+1] + '\n#ifndef Q_OS_WIN' + text[start+1:end] + '#endif\n' + text[end:]

def guard_platform_bodies(text):
    code = lexical(text)
    inserts = []
    for match in re.finditer(r'\bif\s*\(', code):
        paren = code.index('(', match.start())
        end = closing(code, paren, '(', ')')
        condition = code[paren+1:end]
        if ('isPlatformWayland()' in condition and '!KWindowSystem::isPlatformWayland()' not in condition) or 'm_layerWindow' in condition:
            guard = '#ifndef Q_OS_WIN'
        elif 'isPlatformX11()' in condition and 'KX11Extras' not in condition:
            guard = '#if HAVE_X11'
        else:
            continue
        start = end + 1
        while code[start].isspace():
            start += 1
        if code[start] != '{':
            raise RuntimeError('Expected a platform branch body')
        close = closing(code, start, '{', '}')
        close_line = code.rfind('\n', 0, close) + 1
        assert code[close_line:close].strip() == ''
        inserts.extend([(start+1, '\n' + guard), (close_line, '#endif\n')])
    for index, addition in sorted(inserts, reverse=True):
        text = text[:index] + addition + text[index:]
    return text

for name in names:
    target = source / 'shell' / name
    assert target.read_text(encoding='utf-8') == (baseline / 'shell' / name).read_text(encoding='utf-8'), name
    original = subprocess.check_output(['git', 'show', f'HEAD:shell/{name}'], cwd=baseline).decode('utf-8')
    text = original
    if name == 'panelview.cpp':
        text = guard_function(text, 'void PanelView::updateLayerWindow()')
        text = guard_function(text, 'void PanelView::updateExclusiveZone()')
        text = text.replace('    if (!KWindowSystem::isPlatformX11() || KX11Extras::compositingActive()) {', '    bool supportsAlphaMask = true;\n#if HAVE_X11\n    supportsAlphaMask = !KWindowSystem::isPlatformX11() || KX11Extras::compositingActive();\n#endif\n    if (supportsAlphaMask) {')
    if name == 'panelconfigview.cpp':
        text = text.replace('    if (KWindowSystem::isPlatformX11()) {\n#if HAVE_X11\n        KX11Extras::setType(winId(), NET::Dock);\n        KX11Extras::setState(winId(), NET::KeepAbove);', '    if (!KWindowSystem::isPlatformWayland()) {\n#if HAVE_X11\n        KX11Extras::setType(winId(), NET::Dock);\n        KX11Extras::setState(winId(), NET::KeepAbove);\n#endif')
        text = text.replace('#endif\n    } else if (m_layerWindow)', '    } else if (m_layerWindow)', 1)
    text = guard_platform_bodies(text)
    if name == 'desktopview.cpp':
        text = text.replace('        KX11Extras::setType(winId(), NET::Desktop);\n        KX11Extras::setState(winId(), NET::KeepBelow);', '#if HAVE_X11\n        KX11Extras::setType(winId(), NET::Desktop);\n        KX11Extras::setState(winId(), NET::KeepBelow);\n#endif')
    text = text.replace('#include <KX11Extras>', '#if HAVE_X11\n#include <KX11Extras>\n#endif')
    text = text.replace('#if HAVE_X11\n#if HAVE_X11\n#include <KX11Extras>\n#endif\n#endif', '#if HAVE_X11\n#include <KX11Extras>\n#endif')
    text = text.replace('#include <LayerShellQt/Window>', '#ifndef Q_OS_WIN\n#include <LayerShellQt/Window>\n#endif')
    target.write_text(text, encoding='utf-8', newline='\n')
    (baseline / 'shell' / name).write_text(text, encoding='utf-8', newline='\n')

diff = subprocess.check_output(['git', 'diff', '--', *['shell/' + name for name in names]], cwd=baseline)
Path('upstream/plasma-windows/patches/plasma-workspace/0009-windows-shell-platform-guards.patch').write_bytes(diff)
print('Generated shell platform guard patch')
