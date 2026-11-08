# SPDX-License-Identifier: GPL-3.0-or-later
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('normalize_msvc', Path(__file__).resolve().parents[1] / 'tools/build/normalize-msvc.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class DependencyOutput(unittest.TestCase):
    def test_utf8_and_ansi_chinese(self):
        text = '\u6ce8\u610f: \u5305\u542b\u6587\u4ef6:  C:\\\u6d4b\u8bd5\\header.h\r\n'
        expected = 'Note: including file: C:\\\u6d4b\u8bd5\\header.h\r\n'.encode('utf-8')
        for encoding in ('utf-8', 'cp936'):
            self.assertEqual(module.normalized_line(text.encode(encoding)), expected)

    def test_english_and_diagnostics(self):
        text = b'Note: including file:  C:\\SDK\\header.h\r\n'
        self.assertEqual(module.normalized_line(text), b'Note: including file: C:\\SDK\\header.h\r\n')
        error = b'file.cpp(12): error C2001: test\r\n'
        self.assertEqual(module.normalized_line(error), error)


if __name__ == '__main__':
    unittest.main()
