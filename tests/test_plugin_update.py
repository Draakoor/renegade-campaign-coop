import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import update_plugin as update


class PluginUpdateTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.old, self.new = b'MZ-old-plugin', b'MZ-new-plugin'
        for name in ('server/da_campaign.dll', 'release/server/da_campaign.dll'):
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(self.old)
        manifest = {'files': {'da_campaign.dll': update.digest(self.old), 'data/unrelated.mix': 'unchanged'}}
        for name in ('installed.json', 'release/manifest.json'):
            (self.root / name).write_text(json.dumps(manifest))
        self.config = '[General] ; existing settings\nCampaignAdminName=Operator\n  CampaignCatchup=0\n[GameSpyRules]\nOwner=Example\n'
        (self.root / 'server/da.ini').write_text(self.config)
        self.payload = self.root / 'update.zip'

    def package(self, extra=None, tamper=False):
        files = {'server/da_campaign.dll': self.new, 'release/server/da_campaign.dll': self.new}
        files.update(extra or {})
        meta = {'from_sha256': update.digest(self.old),
                'files': {name: update.digest(data) for name, data in files.items()},
                'settings': {'CampaignAdminName': '', 'CampaignCatchup': '1', 'CampaignSkins': '1'}}
        with zipfile.ZipFile(self.payload, 'w') as archive:
            archive.writestr('update.json', json.dumps(meta))
            for name, data in files.items():
                archive.writestr('files/' + name, data + (b'corrupt' if tamper else b''))

    def test_backup_settings_and_idempotency(self):
        self.package({'source/README.md': b'public source'})
        backup = update.apply(self.root, self.payload)
        self.assertEqual((backup / 'server/da_campaign.dll').read_bytes(), self.old)
        self.assertEqual((backup / 'server/da.ini').read_text(), self.config)
        config = (self.root / 'server/da.ini').read_text()
        self.assertIn('CampaignCatchup=0', config)
        self.assertIn('CampaignAdminName=Operator', config)
        self.assertIn('CampaignSkins=1', config)
        self.assertIn('Owner=Example', config)
        manifest = json.loads((self.root / 'installed.json').read_text())
        self.assertEqual(manifest['files']['da_campaign.dll'], update.digest(self.new))
        self.assertEqual(manifest['files']['data/unrelated.mix'], 'unchanged')
        self.assertIsNone(update.apply(self.root, self.payload))
        self.assertEqual(len(list((self.root / 'backups').iterdir())), 1)

    def test_rejects_corruption_and_traversal_before_writes(self):
        for extra, tamper in (({}, True), ({'source/../../escaped': b'x'}, False), ({'runtime.py': b'x'}, False)):
            self.package(extra, tamper)
            with self.assertRaises(ValueError):
                update.apply(self.root, self.payload)
            self.assertEqual((self.root / 'server/da_campaign.dll').read_bytes(), self.old)
            self.assertFalse((self.root / 'backups').exists())

    def test_rejects_unknown_existing_plugin(self):
        self.package()
        (self.root / 'server/da_campaign.dll').write_bytes(b'another build')
        with self.assertRaises(ValueError):
            update.apply(self.root, self.payload)
        self.assertEqual((self.root / 'release/server/da_campaign.dll').read_bytes(), self.old)

    def test_failure_rolls_back_written_files(self):
        self.package()
        replace = update.os.replace
        calls = 0

        def fail_second(source, destination):
            nonlocal calls
            calls += 1
            if calls == 2:
                raise OSError('simulated interrupted write')
            return replace(source, destination)

        with patch.object(update.os, 'replace', fail_second), self.assertRaises(OSError):
            update.apply(self.root, self.payload)
        self.assertEqual((self.root / 'server/da_campaign.dll').read_bytes(), self.old)
        self.assertEqual((self.root / 'release/server/da_campaign.dll').read_bytes(), self.old)
        self.assertEqual((self.root / 'server/da.ini').read_text(), self.config)


if __name__ == '__main__':
    unittest.main()
