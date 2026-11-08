import re

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
