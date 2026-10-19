"""Extract placed actors and scripts from an operator-supplied EA .LVL copy.

Format references: EA CnC_Renegade commit in sources.lock.json,
Tools/LevelEdit/{Node,ObjectNode,EditScript}.cpp. No game data is embedded here.
The FDS omits placed soldiers when loading a single-player map. The plugin
reconstructs missing soldiers with these original transforms and scripts.
"""
import argparse
import math
from pathlib import Path
import struct


def chunks(data):
    pos = 0
    while pos < len(data):
        if len(data) - pos < 8:
            raise ValueError('Truncated chunk header')
        kind, raw_size = struct.unpack_from('<II', data, pos)
        size = raw_size & 0x7fffffff
        end = pos + 8 + size
        if end > len(data):
            raise ValueError('Chunk exceeds its container')
        yield kind, data[pos + 8:end], bool(raw_size & 0x80000000)
        pos = end


def microchunks(data):
    pos = 0
    while pos < len(data):
        if len(data) - pos < 2:
            raise ValueError('Truncated microchunk header')
        kind, size = data[pos:pos + 2]
        end = pos + 2 + size
        if end > len(data):
            raise ValueError('Microchunk exceeds its container')
        yield kind, data[pos + 2:end]
        pos = end


def actors(data, depth=0):
    if depth > 32:
        raise ValueError('Excessive chunk nesting')
    children = list(chunks(data))
    if depth == 0:
        managers = [payload for kind, payload, _ in children if kind == 0x50009]
        if len(managers) != 1:
            raise ValueError('Expected one LevelEdit node manager')
        yield from actors(managers[0], 1)
        return
    bases = [payload for kind, payload, _ in children if kind == 0x10271103]
    if bases:
        fields = {}
        for kind, payload, _ in chunks(bases[0]):
            if kind == 0x100:
                fields.update(microchunks(payload))
        if not all(k in fields for k in (3, 4, 7)):
            raise ValueError('Object node missing preset, ID or transform')
        preset, = struct.unpack('<I', fields[3])
        ident, = struct.unpack('<I', fields[4])
        transform = struct.unpack('<12f', fields[7])
        if not ident or not preset or not all(math.isfinite(x) for x in transform):
            raise ValueError('Invalid actor identity/transform')
        scripts = []
        for kind, payload, _ in children:
            if kind != 0x10271104:
                continue
            name = values = b''
            for subkind, subpayload, _ in chunks(payload):
                if subkind == 0x10271120:
                    params = dict(microchunks(subpayload))
                    name = params.get(1, b'').split(b'\0', 1)[0]
                    values = params.get(3, b'').rstrip(b'\0')
                elif subkind == 0x10271121:
                    values = subpayload.rstrip(b'\0')
            if not name:  # LevelEdit retains unused script slots.
                continue
            if any(c in name for c in (b'\0', b'\n', b'\r')):
                raise ValueError('Invalid script name')
            scripts.append((name, values))
        yield ident, preset, transform, scripts
        return
    for _, payload, nested in children:
        if nested:
            yield from actors(payload, depth + 1)


def compile_source(source, output):
    rows = sorted(actors(Path(source).read_bytes()))
    if not rows or len(rows) > 20000 or len({r[0] for r in rows}) != len(rows):
        raise ValueError('Empty, excessive or duplicate actor table')
    result = bytearray(b'WCM1' + struct.pack('<I', len(rows)))
    for ident, preset, matrix, scripts in rows:
        result += struct.pack('<II12fI', ident, preset, *matrix, len(scripts))
        for script in scripts:
            for value in script:
                if len(value) > 4095:
                    raise ValueError('Oversized script field')
                result += struct.pack('<H', len(value)) + value
    Path(output).write_bytes(result)
    return len(rows)


if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('source', type=Path)
    p.add_argument('output', type=Path)
    args = p.parse_args()
    print(f'Exported {compile_source(args.source, args.output)} original actor placements.')
