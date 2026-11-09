// SPDX-License-Identifier: GPL-3.0-or-later
#include <QCoreApplication>
#include <QTextStream>
#include "inputlayoutlabel.h"
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    bool ok = inputLayoutLabel(MAKELANGID(LANG_GERMAN, SUBLANG_GERMAN)).first == "DE"
        && inputLayoutLabel(MAKELANGID(LANG_FRENCH, SUBLANG_FRENCH)).first == "FR"
        && inputLayoutLabel(MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED)).first == QStringLiteral("中")
        && !inputLayoutLabel(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US)).second.isEmpty();
    QTextStream(stdout)
        << (ok ? "PASS" : "FAIL")
        << ": Windows-localized input labels for English, German, French and Chinese; no layout switched\n";
    return ok ? 0 : 1;
}
