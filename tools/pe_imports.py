# SPDX-License-Identifier: GPL-3.0-or-later
"""Read ordinary x64 PE imports without loading or executing the module."""
import struct

def imports(path):
    data = path.read_bytes()
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    assert data[pe:pe+4] == b'PE\0\0', path
    machine, section_count = struct.unpack_from('<HH', data, pe+4)
    assert machine == 0x8664, f'Not x64: {path}'
    optional_size = struct.unpack_from('<H', data, pe+20)[0]
    optional = pe + 24
    assert struct.unpack_from('<H', data, optional)[0] == 0x20b, path
    import_rva = struct.unpack_from('<I', data, optional+112+8)[0]
    section_base = optional + optional_size
    sections = [struct.unpack_from('<IIII', data, section_base+i*40+8) for i in range(section_count)]
    def offset(rva):
        for virtual_size, address, raw_size, raw_offset in sections:
            if address <= rva < address + max(virtual_size, raw_size):
                return raw_offset + rva - address
        raise ValueError(f'Invalid RVA {rva:x}: {path}')
    if not import_rva:
        return []
    cursor = offset(import_rva)
    result = []
    while any(data[cursor:cursor+20]):
        name_rva = struct.unpack_from('<I', data, cursor+12)[0]
        start = offset(name_rva)
        end = data.index(b'\0', start)
        result.append(data[start:end].decode('ascii'))
        cursor += 20
    return result
