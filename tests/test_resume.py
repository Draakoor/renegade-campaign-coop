import tempfile
from pathlib import Path
import unittest

from runtime import resume_active_mission


class ResumeMissionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.server = Path(self.temp.name)
        (self.server / 'server.ini').write_text('[Server]\nResumeRotation=true\n')
        (self.server / 'tt.cfg').write_text('rotation: ["M00_Tutorial", "M06", "M07", "M09"];\n')
        (self.server / 'resumerotation.txt').write_text('2\n')

    def position(self):
        return (self.server / 'resumerotation.txt').read_text().strip()

    def test_quit_advanced_position_is_replaced_with_last_loaded_map(self):
        (self.server / 'active-map.txt').write_text('m06.MIX\n')
        resume_active_mission(self.server)
        self.assertEqual(self.position(), '1')

    def test_initial_start_and_disabled_resume_preserve_fds_state(self):
        resume_active_mission(self.server)
        self.assertEqual(self.position(), '2')
        (self.server / 'server.ini').write_text('[Server]\nResumeRotation=false\n')
        (self.server / 'active-map.txt').write_text('M06.mix')
        resume_active_mission(self.server)
        self.assertEqual(self.position(), '2')

    def test_removed_map_starts_at_beginning_of_new_rotation(self):
        (self.server / 'active-map.txt').write_text('M11.mix')
        resume_active_mission(self.server)
        self.assertEqual(self.position(), '0')

    def test_invalid_rotation_does_not_change_fds_state(self):
        (self.server / 'active-map.txt').write_text('M06.mix')
        (self.server / 'tt.cfg').write_text('rotation: [];')
        with self.assertRaises(RuntimeError):
            resume_active_mission(self.server)
        self.assertEqual(self.position(), '2')
