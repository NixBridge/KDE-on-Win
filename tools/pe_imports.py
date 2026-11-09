# SPDX-License-Identifier: GPL-3.0-or-later
"""Read ordinary x64/x86 PE imports without loading or executing the module."""
import struct

def architecture(path):
    with path.open('rb') as source:
        source.seek(0x3c)
        pe=struct.unpack('<I',source.read(4))[0]
        source.seek(pe)
        header=source.read(6)
    assert header[:4]==b'PE\0\0',path
    machine=struct.unpack_from('<H',header,4)[0]
    assert machine in (0x8664,0x14c),f'Unsupported PE machine: {path}'
    return 'x64' if machine==0x8664 else 'x86'

def imports(path):
    data = path.read_bytes()
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    assert data[pe:pe+4] == b'PE\0\0', path
    machine, section_count = struct.unpack_from('<HH', data, pe+4)
    assert machine in (0x8664,0x14c), f'Unsupported PE machine: {path}'
    optional_size = struct.unpack_from('<H', data, pe+20)[0]
    optional = pe + 24
    magic=struct.unpack_from('<H',data,optional)[0]
    assert (machine,magic) in ((0x8664,0x20b),(0x14c,0x10b)),path
    directory=112 if magic==0x20b else 96
    import_rva = struct.unpack_from('<I', data, optional+directory+8)[0]
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
