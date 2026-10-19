import struct
import tempfile
from pathlib import Path
import unittest
from tools.mission_sources import actors, chunks, compile_source


def chunk(kind, data, nested=False):
    return struct.pack('<II', kind, len(data) | (0x80000000 if nested else 0)) + data


def micro(kind, data):
    return bytes([kind, len(data)]) + data


def level(ident=123):
    matrix = (1, 0, 0, 10, 0, 1, 0, 20, 0, 0, 1, 30)
    fields = micro(3, struct.pack('<I', 456)) + micro(4, struct.pack('<I', ident)) + micro(7, struct.pack('<12f', *matrix))
    base = chunk(0x10271103, chunk(0x100, fields), True)
    script = chunk(0x10271104, chunk(0x10271120, micro(1, b'M09_Test\0')) + chunk(0x10271121, b'100,200\0'), True)
    node = chunk(0x5000c, chunk(0x100101, base + script, True), True)
    return chunk(0x50009, chunk(0x101, node, True), True)


class MissionSourceTests(unittest.TestCase):
    def test_transform_script_parameters_and_identity_survive(self):
        rows = list(actors(level()))
        self.assertEqual(len(rows), 1)
        ident, preset, transform, scripts = rows[0]
        self.assertEqual((ident, preset), (123, 456))
        self.assertEqual((transform[3], transform[7], transform[11]), (10, 20, 30))
        self.assertEqual(scripts, [(b'M09_Test', b'100,200')])

    def test_compilation_is_deterministic_and_does_not_modify_source(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source = root / 'M09.LVL'
            source.write_bytes(level())
            self.assertEqual(compile_source(source, root / 'one.wcm'), 1)
            compile_source(source, root / 'two.wcm')
            self.assertEqual((root / 'one.wcm').read_bytes(), (root / 'two.wcm').read_bytes())
            self.assertEqual(source.read_bytes(), level())
            self.assertEqual((root / 'one.wcm').read_bytes()[:8], b'WCM1\x01\0\0\0')

    def test_truncated_untrusted_source_is_rejected(self):
        with self.assertRaises(ValueError):
            list(actors(level()[:-3]))
        with self.assertRaises(ValueError):
            list(chunks(b'\x01'))

    def test_invalid_identity_and_duplicate_ids_are_rejected(self):
        with self.assertRaises(ValueError):
            list(actors(level(0)))
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            manager = list(chunks(level()))[0][1]
            node = list(chunks(manager))[0][1]
            src = root / 'bad.LVL'
            src.write_bytes(chunk(0x50009, chunk(0x101, node + node, True), True))
            with self.assertRaises(ValueError):
                compile_source(src, root / 'bad.wcm')
            self.assertFalse((root / 'bad.wcm').exists())
