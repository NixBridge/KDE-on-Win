// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once
#include <QString>
#include <QPair>
#include <windows.h>
inline QPair<QString,QString> inputLayoutLabel(LANGID language)
{
    wchar_t code[32] {},name[256] {};
    const LCID locale=MAKELCID(language,SORT_DEFAULT);
    GetLocaleInfoW(locale,LOCALE_SISO639LANGNAME,code,32);
    GetLocaleInfoW(locale,LOCALE_SLOCALIZEDDISPLAYNAME,name,256);
    QString shortName=QString::fromWCharArray(code).toUpper();
    switch(PRIMARYLANGID(language)) {
    case LANG_CHINESE:shortName=QStringLiteral("中");break;
    case LANG_JAPANESE:shortName=QStringLiteral("日");break;
    case LANG_KOREAN:shortName=QStringLiteral("한");break;
    }
    if(shortName.isEmpty())shortName=QStringLiteral("??");
    return {shortName,QString::fromWCharArray(name)};
}
