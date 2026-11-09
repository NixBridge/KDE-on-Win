# SPDX-License-Identifier: GPL-3.0-or-later
"""Fail when a shipped translation is unfinished or loses format parameters."""
from pathlib import Path
import re
import xml.etree.ElementTree as ET
root=Path(__file__).resolve().parents[1]
catalog=ET.parse(root/'native/translations/kdeonwin_zh_CN.ts').getroot()
messages=catalog.findall('.//message')
assert catalog.get('language')=='zh_CN' and len(messages)>=280
for message in messages:
    source=message.findtext('source')
    translation=message.find('translation')
    assert translation is not None and translation.text and translation.get('type')!='unfinished',source
    assert sorted(re.findall(r'%\d+',source))==sorted(re.findall(r'%\d+',translation.text)),source
print(f'PASS: {len(messages)} finished Chinese translations; all format placeholders preserved.')
