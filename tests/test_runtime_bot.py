import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import Mock, patch

import runtime


class BotStartupTests(unittest.TestCase):
    def test_fresh_bridge_starts_bot_without_early_plugin_log(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            server = root / 'server'
            (server / 'data').mkdir(parents=True)
            (root / '.wine').mkdir()
            (root / '.wine/.novara-runtime-v1').write_text('ready')
            (root / 'installed.json').write_text('{}')
            bridge = server / 'brenbot-events.jsonl'
            bridge.write_text('old data\n')
            previous = bridge.stat().st_mtime_ns

            class Game:
                returncode = 0
                polls = 0

                def poll(self):
                    self.polls += 1
                    if self.polls == 1:
                        # First poll still sees yesterday's bridge file.
                        return None
                    if self.polls == 2:
                        start.assert_not_called()
                        bridge.write_text(json.dumps({'event': 'players', 'players': []}) + '\n')
                        import os
                        os.utime(bridge, ns=(previous + 1_000_000, previous + 1_000_000))
                        (server / 'panel-console.log').write_text('Level M01.mix Loaded OK\n')
                        return None
                    return 0

            bot = Mock()
            bot.poll.return_value = None
            with patch.object(runtime, 'ROOT', root), \
                 patch.object(runtime, 'publication_mode', return_value='development'), \
                 patch.object(runtime, 'validate_settings'), \
                 patch.object(runtime, 'resume_active_mission'), \
                 patch.object(runtime, 'package_directory', return_value=root), \
                 patch.object(runtime, 'ThreadingHTTPServer'), \
                 patch.object(runtime.threading, 'Thread'), \
                 patch.object(runtime.signal, 'signal'), \
                 patch.object(runtime.time, 'sleep'), \
                 patch.object(runtime.subprocess, 'Popen', return_value=Game()), \
                 patch('brenbot_runtime.start', return_value=bot) as start, \
                 patch('brenbot_runtime.stop') as stop:
                runtime.run_locked()
            start.assert_called_once_with(root)
            stop.assert_called_once_with(bot)


if __name__ == '__main__':
    unittest.main()
