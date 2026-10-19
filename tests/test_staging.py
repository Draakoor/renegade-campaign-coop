import os
from pathlib import Path
import tempfile
import unittest
import zipfile
from tools.fetch_inputs import safe_extract
from tools.stage_volume import check_inputs
from volume_lock import volume_lock


class StagingTests(unittest.TestCase):
    def test_missing_assets_fail_before_download(self):
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaisesRegex(RuntimeError, 'CoopBeta424.zip'):
                check_inputs(Path(tmp))
            self.assertEqual(list(Path(tmp).iterdir()), [])

    def test_archive_traversal_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            archive = root / 'bad.zip'
            with zipfile.ZipFile(archive, 'w') as zipped:
                zipped.writestr('../outside.txt', 'bad')
            with self.assertRaises(ValueError):
                safe_extract(archive, root / 'extract')
            self.assertFalse((root / 'outside.txt').exists())

    def test_lock_refuses_concurrent_owner_and_is_reusable(self):
        with tempfile.TemporaryDirectory() as tmp:
            with volume_lock(tmp):
                with self.assertRaises(RuntimeError):
                    with volume_lock(tmp):
                        self.fail('Concurrent lock acquired')
            with volume_lock(tmp):
                self.assertTrue((Path(tmp) / '.operation.lock').is_file())
