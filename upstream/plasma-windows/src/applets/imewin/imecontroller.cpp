/*
    SPDX-FileCopyrightText: 2026 Plasma Windows contributors
    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "imecontroller.h"

#include <QDebug>
#include <windows.h>
#include "../../shared/inputlayoutlabel.h"
#include <QVector>

ImeController::ImeController(QObject *parent)
    : QObject(parent)
{
    connect(&m_timer, &QTimer::timeout, this, &ImeController::refresh);
    m_timer.start(300);
    refresh();
}

QString ImeController::layoutName() const
{
    return m_name;
}

QString ImeController::layoutCode() const
{
    return m_code;
}

void ImeController::refresh()
{
    // The IME of the *foreground* window's thread, not ours.
    const HWND fg = GetForegroundWindow();
    // Remember the real user window: when the panel is clicked, the
    // foreground window is our own shell, but the layout switch must be
    // sent to the application the user actually uses.
    if (fg) {
        DWORD fgPid = 0;
        const DWORD fgThread=GetWindowThreadProcessId(fg, &fgPid);
        if (fgPid != GetCurrentProcessId()) {
            m_targetWnd = fg;
            m_targetPid=fgPid;
            m_targetThread=fgThread;
        }
    }
    // Read the layout of the user window (m_targetWnd), not the shell's:
    // after a click the foreground is the panel itself, whose thread has
    // its own HKL that never changes - that made the indicator stick.
    DWORD pid=0;
    const DWORD owner=GetWindowThreadProcessId(m_targetWnd,&pid);
    const HWND probe = (owner==m_targetThread && pid==m_targetPid && owner) ? m_targetWnd : fg;
    const DWORD tid = probe ? GetWindowThreadProcessId(probe, nullptr) : 0;
    const HKL hkl = GetKeyboardLayout(tid);
    const LANGID lang = LOWORD(hkl);
    const auto label=inputLayoutLabel(lang);
    const auto& code=label.first;
    const auto& name=label.second;

    if (code != m_code || name != m_name) {
        m_code = code;
        m_name = name;
        Q_EMIT layoutNameChanged();
    }
}

void ImeController::toggle()
{
    const int capacity=GetKeyboardLayoutList(0,nullptr);
    if(capacity<=0 || capacity>256)return;
    QVector<HKL> list(capacity);
    const int count = GetKeyboardLayoutList(capacity, list.data());
    if (count <= 0) {
        return;
    }
    // Prefer the last real user window; fall back to the current
    // foreground (which after clicking our panel is the shell itself).
    HWND target = m_targetWnd;
    DWORD pid=0; DWORD thread=GetWindowThreadProcessId(target,&pid);
    if (!thread || pid!=m_targetPid || thread!=m_targetThread) target=GetForegroundWindow();
    GetWindowThreadProcessId(target,&pid);
    if (!target || pid==GetCurrentProcessId()) {
        return;
    }
    const HKL cur = GetKeyboardLayout(GetWindowThreadProcessId(target, nullptr));
    HKL next = list[0];
    for (int i = 0; i < count; ++i) {
        if (list[i] == cur) {
            next = list[(i + 1) % count];
            break;
        }
    }
    PostMessageW(target, WM_INPUTLANGCHANGEREQUEST, 0, reinterpret_cast<LPARAM>(next));
    refresh();
}

#include "moc_imecontroller.cpp"
