"""Build a Win32 plugin without rebuilding or replacing the upstream engine.

Run from a VS x86 developer environment. Import libraries are generated from
the exact release PE exports, including DATA exports for imported globals.
"""
import argparse
import pathlib
import struct
import subprocess


def import_def(dll):
    data = dll.read_bytes()
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    machine, count = struct.unpack_from('<HH', data, pe + 4)
    if machine != 0x14c:
        raise ValueError(f'{dll}: expected x86')
    opt_size = struct.unpack_from('<H', data, pe + 20)[0]
    opt = pe + 24
    export_rva, export_size = struct.unpack_from('<II', data, opt + 96)
    sections = []
    for i in range(count):
        s = opt + opt_size + 40 * i
        virtual_size, rva, raw_size, offset = struct.unpack_from('<IIII', data, s + 8)
        flags = struct.unpack_from('<I', data, s + 36)[0]
        sections.append((rva, max(virtual_size, raw_size), offset, flags))

    def address(rva):
        for start, size, offset, _ in sections:
            if start <= rva < start + size:
                return offset + rva - start
        raise ValueError(f'Unmapped RVA {rva:x}')

    export = address(export_rva)
    base, nfunc, nname, funcs, names, ordinals = struct.unpack_from('<IIIIII', data, export + 16)
    lines = [f'LIBRARY "{dll.name}"', 'EXPORTS']
    for i in range(nname):
        name_rva = struct.unpack_from('<I', data, address(names) + 4 * i)[0]
        start = address(name_rva)
        name = data[start:data.index(b'\0', start)].decode('ascii')
        ordinal = struct.unpack_from('<H', data, address(ordinals) + 2 * i)[0]
        target = struct.unpack_from('<I', data, address(funcs) + 4 * ordinal)[0]
        code = any(s <= target < s + z and flags & 0x20000000 for s, z, _, flags in sections)
        forwarded = export_rva <= target < export_rva + export_size
        lines.append(f'  {name} @{base + ordinal}' + ('' if code or forwarded else ' DATA'))
    return '\n'.join(lines) + '\n'


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--dragonade', type=pathlib.Path, required=True)
    p.add_argument('--output', type=pathlib.Path, required=True)
    args = p.parse_args()
    root = args.dragonade.resolve()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    libs = []
    for name in ['da', 'scripts', 'MemoryManager', 'shared']:
        dll = root / 'Server' / (name + '.dll')
        deffile = out / (name + '.def')
        lib = out / (name + '.lib')
        deffile.write_text(import_def(dll), encoding='ascii')
        subprocess.run(['lib', '/nologo', '/machine:x86', f'/def:{deffile}', f'/out:{lib}'], check=True)
        libs.append(str(lib))
    source = pathlib.Path(__file__).resolve().parents[1] / 'plugin' / 'campaign.cpp'
    subprocess.run(['cl', '/nologo', '/LD', '/O2', '/MD', '/std:c++14', '/Zc:twoPhase-',
                    '/GR-', '/EHsc', '/DWIN32', '/DNDEBUG', '/DDAPLUGIN',
                    '/D_CRT_SECURE_NO_WARNINGS', '/D_CRT_NONSTDC_NO_DEPRECATE',
                    '/D_SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS',
                    '/I' + str(root / 'Source' / 'scripts'),
                    '/I' + str(root / 'Source' / 'MemoryManager'),
                    '/Fo' + str(out / 'campaign.obj'), str(source),
                    '/link', '/MACHINE:X86', '/OUT:' + str(out / 'da_campaign.dll'),
                    '/IMPLIB:' + str(out / 'da_campaign.lib'),
                    *libs, 'ws2_32.lib', 'shlwapi.lib', 'winmm.lib'], check=True)


if __name__ == '__main__':
    main()
