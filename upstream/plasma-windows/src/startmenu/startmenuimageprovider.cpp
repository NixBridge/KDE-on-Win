/*
    SPDX-FileCopyrightText: 2026 Plasma Windows contributors
    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "startmenuimageprovider.h"

#include <QFileInfo>
#include <QDir>
#include <QUrl>
#include <QMimeDatabase>
#include <KIconLoader>

#include <shlobj.h>

namespace
{
QImage hbmpToImage(HBITMAP hbmp)
{
    BITMAP bmp{};
    GetObjectW(hbmp, sizeof(bmp), &bmp);
    if (bmp.bmWidth <= 0 || bmp.bmHeight <= 0) {
        return {};
    }
    const int w = bmp.bmWidth;
    const int h = bmp.bmHeight;
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    QImage out(w, h, QImage::Format_ARGB32);
    HDC dc = GetDC(nullptr);
    const int lines = GetDIBits(dc, hbmp, 0, h, out.bits(), &bmi, DIB_RGB_COLORS);
    ReleaseDC(nullptr, dc);
    if (lines != h) {
        return {};
    }
    return out;
}

// Render an HICON into an ARGB32 QImage using the system's own icon
// compositing (DrawIconEx handles both 32-bit alpha and legacy mask
// icons), avoiding all-black results from naive mask interpretation.
QImage iconToImage(HICON hIcon)
{
    return hIcon ? QImage::fromHICON(hIcon) : QImage();
}
} // namespace

StartMenuImageProvider::StartMenuImageProvider()
    : QQuickImageProvider(QQmlImageProviderBase::Image)
{
}

QImage StartMenuImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    Q_UNUSED(requestedSize)

    QString lnkPath = QUrl::fromPercentEncoding(id.toUtf8());
    const bool breeze = lnkPath.startsWith(QLatin1String("breeze:"));
    if (breeze) lnkPath.remove(0, 7);
    if (lnkPath.startsWith(QLatin1String("file:"))) {
        lnkPath = QUrl(lnkPath).toLocalFile();
    }
    if (!lnkPath.startsWith(QLatin1String("apps:"))) {
        lnkPath = QDir::toNativeSeparators(lnkPath);
    }
    if (breeze) {
        const QFileInfo file(lnkPath);
        QString name;
        if (file.isDir()) name = QStringLiteral("folder");
        else if (lnkPath.startsWith(QLatin1String("apps:")) || file.suffix().compare(QLatin1String("lnk"), Qt::CaseInsensitive) == 0
                 || file.suffix().compare(QLatin1String("exe"), Qt::CaseInsensitive) == 0)
            name = QStringLiteral("application-x-executable");
        else name = QMimeDatabase().mimeTypeForFile(file).iconName();
        const int pixels = requestedSize.width() > 0 ? requestedSize.width() : 64;
        QImage image = KIconLoader::global()->loadIcon(name, KIconLoader::Desktop, pixels).toImage();
        if (size) *size = image.size();
        return image;
    }

    auto it = m_cache.constFind(lnkPath);
    if (it == m_cache.constEnd()) {
        it = m_cache.insert(lnkPath, iconForLink(lnkPath));
    }
    if (size) {
        *size = it->size();
    }
    return it.value();
}

QImage StartMenuImageProvider::iconForLink(const QString &lnkPath) const
{
    /* UWP apps: linkPath is "apps:<AUMID>" - extract the icon via the
       AppsFolder IShellItemImageFactory */
    if (lnkPath.startsWith(QLatin1String("apps:"))) {
        const QString aumid = lnkPath.mid(5);
        const QString shellPath = QStringLiteral("shell:AppsFolder\\%1").arg(aumid);
        IShellItem *item = nullptr;
        if (SUCCEEDED(SHCreateItemFromParsingName(reinterpret_cast<LPCWSTR>(shellPath.utf16()), nullptr,
                                                  IID_IShellItem, reinterpret_cast<void **>(&item)))) {
            IShellItemImageFactory *factory = nullptr;
            if (SUCCEEDED(item->QueryInterface(IID_IShellItemImageFactory, reinterpret_cast<void **>(&factory)))) {
                SIZE size{32, 32};
                HBITMAP hbmp = nullptr;
                if (SUCCEEDED(factory->GetImage(size, SIIGBF_ICONONLY | SIIGBF_BIGGERSIZEOK, &hbmp))) {
                    const QImage img = hbmpToImage(hbmp);
                    DeleteObject(hbmp);
                    factory->Release();
                    item->Release();
                    return img;
                }
                factory->Release();
            }
            item->Release();
        }
        return {};
    }

    // Use the Shell association for folders, archives and shortcuts, including
    // overlays. Qt handles both alpha icons and legacy AND/XOR icon masks.
    SHFILEINFOW info{};
    if (SHGetFileInfoW(reinterpret_cast<LPCWSTR>(lnkPath.utf16()), 0, &info,
                       sizeof(info), SHGFI_ICON | SHGFI_LARGEICON)) {
        const QImage image = iconToImage(info.hIcon);
        if (info.hIcon) DestroyIcon(info.hIcon);
        if (!image.isNull()) return image;
    }

    // 1. explicit icon location from the .lnk (usually the exe + index)
    QString iconSource;
    int iconIndex = 0;
    QString exec;
    IShellLinkW *shellLink = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, reinterpret_cast<void **>(&shellLink)))) {
        IPersistFile *persist = nullptr;
        if (SUCCEEDED(shellLink->QueryInterface(IID_IPersistFile, reinterpret_cast<void **>(&persist)))) {
            if (SUCCEEDED(persist->Load(reinterpret_cast<LPCWSTR>(lnkPath.utf16()), STGM_READ))) {
                wchar_t path[MAX_PATH] = {};
                if (SUCCEEDED(shellLink->GetPath(path, MAX_PATH, nullptr, 0))) {
                    exec = QString::fromWCharArray(path);
                }
                wchar_t iconBuf[MAX_PATH] = {};
                int idx = 0;
                if (SUCCEEDED(shellLink->GetIconLocation(iconBuf, MAX_PATH, &idx))) {
                    iconSource = QString::fromWCharArray(iconBuf);
                    iconIndex = idx;
                }
            }
            persist->Release();
        }
        shellLink->Release();
    }

    HICON hIcon = nullptr;
    if (!iconSource.isEmpty() && QFileInfo::exists(iconSource)) {
        const UINT n = ExtractIconExW(reinterpret_cast<LPCWSTR>(iconSource.utf16()), iconIndex, &hIcon, nullptr, 1);
        if (n == 0) {
            hIcon = nullptr;
        }
    }
    if (!hIcon) {
        hIcon = static_cast<HICON>(ExtractIconW(nullptr, reinterpret_cast<LPCWSTR>(lnkPath.utf16()), 0));
    }
    if (!hIcon && !exec.isEmpty()) {
        const UINT n = ExtractIconExW(reinterpret_cast<LPCWSTR>(exec.utf16()), 0, &hIcon, nullptr, 1);
        if (n == 0) {
            hIcon = nullptr;
        }
    }

    QImage img;
    if (hIcon) {
        img = iconToImage(hIcon);
        DestroyIcon(hIcon);
    }
    return img;
}
