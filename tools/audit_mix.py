"""Read the MIX1 filename table without modifying original game archives."""
import argparse
import json
from pathlib import Path
import struct


def names(path):
    with Path(path).open('rb') as f:
        magic, _, name_offset = struct.unpack('<4sII', f.read(12))
        if magic != b'MIX1':
            raise ValueError('Expected Renegade MIX1 archive')
        f.seek(name_offset)
        count, = struct.unpack('<I', f.read(4))
        if count > 100000:
            raise ValueError('Invalid file count')
        result = []
        for _ in range(count):
            length, = struct.unpack('<B', f.read(1))
            result.append(f.read(length).rstrip(b'\0').decode('cp1252'))
        return result


if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('archive', type=Path)
    print(json.dumps(names(p.parse_args().archive), indent=2))
