"""Exercise the real Perl BRenBot + plugins against an isolated file bridge."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import time

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO))
from brenbot_runtime import prepare, stop


def test(core):
    with tempfile.TemporaryDirectory(prefix='brenbot-test-') as temp:
        root = Path(temp)
        server = root / 'server'
        (server / 'data').mkdir(parents=True)
        (server / 'data/svrcfg_cnc.ini').write_text('[Settings]\nMapName=M01.mix\nMaxPlayers=8\n')
        (server / 'tt.cfg').write_text('rotation:\n[\n"M01"\n];\n')
        (server / 'active-map.txt').write_text('M01.mix\n')
        events = server / 'brenbot-events.jsonl'
        events.touch()
        output = server / 'brenbot.in'
        output.touch()
        work = prepare(root, core)
        # Short, deterministic cooldowns exercise the production plugin logic.
        xml = work / 'plugins/funsounds.xml'
        xml.write_text(xml.read_text().replace('value="10"', 'value="0.2"').replace('value="3"', 'value="0.1"'))
        env = os.environ.copy()
        env.update(NOVARA_SERVER=str(server), PERL5LIB=os.pathsep.join((str(core), str(REPO / 'tests/perl'))), PERL5OPT='-MNovaraNoNetwork')
        proc = subprocess.Popen(['perl', str(core / 'brenbot.pl')], cwd=work, env=env,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        log = []
        thread = threading.Thread(target=lambda: log.extend(iter(proc.stdout.readline, '')), daemon=True)
        thread.start()

        def wait(predicate, seconds=10):
            until = time.monotonic() + seconds
            while time.monotonic() < until:
                if predicate():
                    return
                if proc.poll() is not None:
                    raise AssertionError('BRenBot exited:\n' + ''.join(log))
                time.sleep(.05)
            raise AssertionError('BRenBot timeout:\n' + ''.join(log) + '\nCommands:\n' + output.read_text())

        def send(row):
            with events.open('a') as stream:
                stream.write(json.dumps(row) + '\n')

        players = [dict(id=1, session=11, name='Draakoor', team=1), dict(id=2, session=22, name='Teammate', team=1), dict(id=3, session=33, name='NodTest', team=0)]

        def chat(message, index=0, scope='public'):
            send(dict(event='chat', scope=scope, message=message, **players[index]))

        try:
            wait(lambda: 'BRenBot ready:' in ''.join(log))
            send(dict(event='players', players=players))
            chat('!version')
            wait(lambda: 'BRenBot 1.56' in output.read_text())
            chat('!help')
            wait(lambda: 'Coop: !buy' in output.read_text())
            assert output.read_text().count('Coop: !buy') == 1
            chat('!sounds')
            wait(lambda: 'Sounds 1/' in output.read_text())
            chat('!sounds off', 1)
            wait(lambda: 'sounds off for you' in output.read_text())
            before = len(output.read_text())
            chat('!taunt haha', scope='team')
            wait(lambda: 'sound 11 1 laugh1.wav' in output.read_text()[before:])
            emitted = output.read_text()[before:]
            assert 'sound 22 2 ' not in emitted and 'sound 33 3 ' not in emitted
            chat('!taunt haha', scope='team')
            wait(lambda: 'cooldown active' in output.read_text()[before:])
            time.sleep(.3)
            send(dict(event='leave', **players[1]))
            players[1]['session'] = 24
            send(dict(event='join', **players[1]))
            before = len(output.read_text())
            chat('well played gg')
            wait(lambda: 'sound 24 2 ' in output.read_text()[before:])
            assert 'sound 33 3 ' in output.read_text()[before:]
            # No duplicate/game admin handlers, no command or newline injection.
            before = len(output.read_text())
            chat('!givecredits me 1000')
            chat('!map M09')
            chat('!taunt haha\nquit')
            chat('a word containing giggle')
            time.sleep(.4)
            assert 'sound ' not in output.read_text()[before:]
            assert not any(x.startswith(('quit', 'kick', 'gameover', 'campaign_admin')) for x in output.read_text().splitlines())
            assert 'TEST: BRenBot attempted' not in ''.join(log)
            assert 'Error' not in ''.join(log), ''.join(log)
            # Re-running setup preserves operator configuration and bot state.
            custom = (work / 'funsounds.json').read_bytes()
            prepare(root, core)
            assert (work / 'funsounds.json').read_bytes() == custom
            print('PASS: real BRenBot, both plugins, unified help, version, sounds, team scope, mute/reconnect, cooldown, command isolation, no network.')
        finally:
            stop(proc)
            thread.join(timeout=2)
            print(''.join(log))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--core', type=Path, required=True)
    test(parser.parse_args().core.resolve())
