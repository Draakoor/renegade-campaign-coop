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

    def package(self, extra=None, tamper=False, migration=None):
        files = {'server/da_campaign.dll': self.new, 'release/server/da_campaign.dll': self.new}
        files.update(extra or {})
        meta = {'from_sha256': update.digest(self.old),
                'files': {name: update.digest(data) for name, data in files.items()},
                'settings': {'CampaignAdminName': '', 'CampaignCatchup': '1', 'CampaignSkins': '1'}}
        if migration:
            meta['migration'] = migration
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

    def test_explicit_brenbot_migration_removes_tutorial_and_preserves_unrelated_settings(self):
        (self.root / 'server/da.ini').write_text(self.config + '\n[M06.mix]\nEnableChatSounds=1\nEnableTaunts=1\nEnableDonate=1\nCampaignAdaptiveDifficulty=1\n')
        old_tt = 'gameDefinitions: { M00_Tutorial: { mapName="M00_Tutorial"; }; };\nrotation: ["M00_Tutorial", "M06"];\ndownloader: { repositoryUrl="http://example:35002/"; };\n'
        old_cfg = '[Settings]\nbGameTitle=Custom name\nMapName=M00_Tutorial.mix\nMapName00=M00_Tutorial.mix\nMapName01=M06.mix\n[Other]\nKeep=1\n'
        (self.root / 'server/tt.cfg').write_text(old_tt)
        (self.root / 'server/data').mkdir()
        (self.root / 'server/data/svrcfg_cnc.ini').write_text(old_cfg)
        self.package({'runtime.py': b'supervisor', 'brenbot_runtime.py': b'bot'}, migration='novara-brenbot-m01-v1')
        backup = update.apply(self.root, self.payload)
        self.assertEqual((backup / 'server/tt.cfg').read_text(), old_tt)
        self.assertEqual((backup / 'server/data/svrcfg_cnc.ini').read_text(), old_cfg)
        current = (self.root / 'server/tt.cfg').read_text()
        self.assertIn('"M01", "M02"', current)
        self.assertIn('http://example:35002/', current)
        cfg = (self.root / 'server/data/svrcfg_cnc.ini').read_text()
        self.assertNotIn('M00_Tutorial', cfg)
        self.assertIn('MapName10=M11.mix', cfg)
        self.assertIn('bGameTitle=Custom name', cfg)
        self.assertIn('[Other]\nKeep=1', cfg)
        settings = (self.root / 'server/da.ini').read_text()
        for key in ('EnableChatSounds', 'EnableTaunts', 'EnableDonate'):
            self.assertIn(key + '=0', settings)
            self.assertNotIn(key + '=1', settings)
        self.assertNotIn('CampaignAdaptiveDifficulty', settings)
        self.assertIsNone(update.apply(self.root, self.payload))

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
