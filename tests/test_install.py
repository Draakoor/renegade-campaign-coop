import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def load(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / (name + '.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


bootstrap = load('bootstrap')
runtime = load('runtime')


class InstallationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / 'release' / 'server').mkdir(parents=True)
        (self.root / 'defaults').mkdir()
        (self.root / 'defaults' / 'server.ini').write_text('[Server]\nPort=35000\n')
        self.binary = self.root / 'release' / 'server' / 'server.exe'
        self.binary.write_bytes(b'version-one')
        self.write_manifest()

    def write_manifest(self):
        (self.root / 'release' / 'manifest.json').write_text(json.dumps({'files': {'server.exe': bootstrap.sha(self.binary)}}))

    def test_reinstall_preserves_settings_and_backs_up_changed_binary(self):
        bootstrap.install(self.root)
        config = self.root / 'server' / 'server.ini'
        config.write_text('[Server]\nPort=35007\n')
        self.binary.write_bytes(b'version-two')
        self.write_manifest()
        bootstrap.install(self.root)
        self.assertEqual(config.read_text(), '[Server]\nPort=35007\n')
        self.assertEqual((self.root / 'server' / 'server.exe').read_bytes(), b'version-two')
        backups = list((self.root / 'backups').rglob('server.exe'))
        self.assertEqual(len(backups), 1)
        self.assertEqual(backups[0].read_bytes(), b'version-one')

    def test_corruption_is_rejected_before_live_changes(self):
        self.binary.write_bytes(b'tampered')
        with self.assertRaises(RuntimeError):
            bootstrap.install(self.root)
        self.assertFalse((self.root / 'server').exists())

    def test_incomplete_missions_cannot_be_public(self):
        (self.root / 'validation.json').write_text(json.dumps({'missions': {}}))
        with self.assertRaisesRegex(RuntimeError, 'M00_Tutorial'):
            runtime.check_validation(self.root)


if __name__ == '__main__':
    unittest.main()
