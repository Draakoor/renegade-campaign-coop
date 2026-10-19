import configparser
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import gamespy
from tools.set_public import configure


class PublicationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / 'server/data').mkdir(parents=True)
        self.game = self.root / 'server/data/svrcfg_cnc.ini'
        self.game.write_text('[Settings]\nIsPassworded=yes\nbPassword=private-test\nbMotd=Private\nMaxPlayers=8\n')
        self.da = self.root / 'server/da.ini'
        self.da.write_text('[General]\nEnableGameSpy=1\nGameSpyMasterServers=127.0.0.1\nGameSpyQueryPort=35001\nCampaignDiagnostics=1\n')

    def settings(self):
        game, da = configparser.ConfigParser(), configparser.ConfigParser()
        game.read(self.game)
        da.read(self.da)
        return game, da

    def test_private_by_default_and_requires_password(self):
        self.assertEqual(gamespy.publication_mode(self.root), 'development')
        game, da = self.settings()
        gamespy.validate_settings('development', game, da)
        game.set('Settings', 'IsPassworded', 'no')
        with self.assertRaisesRegex(RuntimeError, 'password'):
            gamespy.validate_settings('development', game, da)

    def test_public_opt_in_backs_up_and_can_revert_without_acceptance_changes(self):
        evidence = self.root / 'validation.json'
        evidence.write_text('{"missions": {}}')
        old = self.game.read_bytes()
        backup = configure(self.root, 'public-test')
        self.assertEqual((backup / 'server/data/svrcfg_cnc.ini').read_bytes(), old)
        self.assertEqual(gamespy.publication_mode(self.root), 'public-test')
        game, da = self.settings()
        gamespy.validate_settings('public-test', game, da)
        self.assertFalse(da.getboolean('General', 'CampaignDiagnostics'))
        self.assertEqual(game.getint('Settings', 'MaxPlayers'), 8)
        self.assertEqual(evidence.read_text(), '{"missions": {}}')
        configure(self.root, 'development')
        game, da = self.settings()
        gamespy.validate_settings('development', game, da)
        self.assertEqual(game.get('Settings', 'bPassword'), 'private-test')

    def test_bad_mode_or_legacy_public_broadcaster_is_rejected(self):
        (self.root / 'publication.json').write_text(json.dumps({'mode': 'release'}))
        with self.assertRaises(RuntimeError):
            gamespy.publication_mode(self.root)
        game, da = self.settings()
        da.set('General', 'GameSpyMasterServers', 'example.org')
        with self.assertRaisesRegex(RuntimeError, 'legacy'):
            gamespy.validate_settings('development', game, da)

    def test_heartbeat_uses_tt_query_port_and_terminator(self):
        self.assertEqual(gamespy.heartbeat_packet(), b'\\heartbeat\\35001\\gamename\\ccrenegade\\final')
        self.assertLessEqual(gamespy.INTERVAL, 30)

    def test_no_advertisement_before_query_ready(self):
        from threading import Event
        stopped = Event()
        with patch('gamespy.socket.socket') as sock, patch('gamespy.query_ready', return_value=False), \
                patch.object(stopped, 'wait', side_effect=lambda _: stopped.set()):
            gamespy.broadcast(stopped)
            sock.return_value.__enter__.return_value.sendto.assert_not_called()

    def test_master_acknowledgement_and_dns_alias_deduplication(self):
        from threading import Event
        stopped = Event()
        address = ('192.0.2.1', 27900)
        def reply(_):
            stopped.set()
            return b'\\heartbeatresult\\0\\final', address
        with patch('gamespy.socket.socket') as factory, patch('gamespy.query_ready', return_value=True), \
                patch('gamespy.socket.getaddrinfo', return_value=[(None, None, None, None, address)]), \
                patch('builtins.print') as output:
            sock = factory.return_value.__enter__.return_value
            sock.recvfrom.side_effect = reply
            gamespy.broadcast(stopped, masters=('master.example', 'alias.example'))
            sock.sendto.assert_called_once_with(gamespy.heartbeat_packet(), address)
            output.assert_called_once_with('[GameSpy] master.example: listed (heartbeatresult=0).', flush=True)


if __name__ == '__main__':
    unittest.main()
