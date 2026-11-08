"""Generate native window fixes for the remaining compiled workspace views."""
import importlib.util
import subprocess
from pathlib import Path

spec = importlib.util.spec_from_file_location('guards', Path(__file__).with_name('source-platform-guards.py'))
guards = importlib.util.module_from_spec(spec)
spec.loader.exec_module(guards)
source = Path('D:/KDE/CraftRoot/build/kde/plasma/plasma-workspace/work/plasma-workspace-6.7.4')
baseline = Path('D:/KDE/sources/plasma-workspace-repair/plasma-workspace-6.7.4')
names = ['krunner/view.cpp', 'ksplash/ksplashqml/splashwindow.cpp', 'applets/kicker/dashboardwindow.cpp', 'applets/kicker/windowsystem.cpp', 'applets/notifications/notificationapplet.cpp', 'applets/notifications/notificationwindow.cpp']
for name in names:
    target = source / name
    original = target.read_text(encoding='utf-8')
    assert original == (baseline / name).read_text(encoding='utf-8'), name
    original = subprocess.check_output(['git', 'show', f'HEAD:{name}'], cwd=baseline).decode('utf-8')
    text = original
    if '"config-X11.h"' not in text:
        index = text.index('#include ')
        text = text[:index] + '#include "config-X11.h"\n' + text[index:]
    if name == 'krunner/view.cpp':
        text = text.replace('} else if (KWindowSystem::isPlatformX11()) {\n        screenIt', '} else if (!KWindowSystem::isPlatformWayland()) {\n        screenIt', 1)
        needle = '        KX11Extras::setState(winId(), NET::SkipTaskbar | NET::SkipPager);\n    }\n}'
        replacement = '''        KX11Extras::setState(winId(), NET::SkipTaskbar | NET::SkipPager);
    }
#ifdef Q_OS_WIN
    else {
        const QRect available = shownOnScreen->availableGeometry();
        const int top = available.top() + (m_floating ? available.height() / 3 : 0);
        setPosition(QPoint(available.left() + (available.width() - width()) / 2, top));
    }
#endif
}'''
        assert needle in text
        text = text.replace(needle, replacement, 1)
    if name == 'applets/kicker/windowsystem.cpp':
        needle = '        if (auto window = item->window(); KWindowSystem::isPlatformX11() && window) {\n            KX11Extras::forceActiveWindow(window->winId());\n        }'
        replacement = '''        if (auto window = item->window()) {
#ifdef Q_OS_WIN
            KWindowSystem::activateWindow(window);
#else
            if (KWindowSystem::isPlatformX11()) {
                KX11Extras::forceActiveWindow(window->winId());
            }
#endif
        }'''
        assert needle in text
        text = text.replace(needle, replacement)
    if name == 'applets/notifications/notificationapplet.cpp':
        text = text.replace('#include <KX11Extras>', '#include <KWindowSystem>\n#include <KX11Extras>')
        text = text.replace('        KX11Extras::forceActiveWindow(window->winId());', '#ifdef Q_OS_WIN\n        KWindowSystem::activateWindow(window);\n#else\n        KX11Extras::forceActiveWindow(window->winId());\n#endif')
    text = guards.guard_platform_bodies(text)
    text = text.replace('#include <KX11Extras>', '#if HAVE_X11\n#include <KX11Extras>\n#endif')
    text = text.replace('#include <LayerShellQt/Window>', '#ifndef Q_OS_WIN\n#include <LayerShellQt/Window>\n#endif')
    target.write_text(text, encoding='utf-8', newline='\n')
    (baseline / name).write_text(text, encoding='utf-8', newline='\n')
diff = subprocess.check_output(['git', 'diff', '--', *names], cwd=baseline)
Path('upstream/plasma-windows/patches/plasma-workspace/0010-windows-runner-notification-views.patch').write_bytes(diff)
print('Generated runner, splash and notification view patch')
