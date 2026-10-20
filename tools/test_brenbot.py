"""Exercise the real Perl BRenBot + plugins against an isolated file bridge."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import sqlite3
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
        xml.write_text(xml.read_text().replace('value="10"', 'value="1"').replace('value="3"', 'value="0.3"'))
        xml = work / 'plugins/jukebox.xml'
        xml.write_text(xml.read_text().replace('value="2"', 'value="0.3"'))
        xml = work / 'plugins/fds_status_reporter.xml'
        xml.write_text(xml.read_text().replace('value="15"', 'value="1"'))
        # Exercise catalog migration from an existing operator configuration.
        original_sounds = json.loads((work / 'funsounds.json').read_text())
        original_sounds['hilfe'] = 'beep.wav'
        (work / 'funsounds.json').write_text(json.dumps(original_sounds))
        tracks = json.loads((core / 'jukebox.json').read_text())
        assert len(tracks) == 19
        tracks[0]['seconds'] = 3
        (work / 'jukebox.json').write_text(json.dumps(tracks))
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
            raise AssertionError('BRenBot timeout:\n' + ''.join(log) + '\nCommands:\n' + output.read_text(encoding='cp1252'))

        def send(row):
            with events.open('a') as stream:
                stream.write(json.dumps(row) + '\n')

        players = [dict(id=1, session=11, name='Draakoor', team=1, ping=42, object=1001),
                   dict(id=2, session=22, name='Teammate', team=1, ping=55, object=1002),
                   dict(id=3, session=33, name='NodTest', team=0, ping=65, object=1003)]

        def chat(message, index=0, scope='public'):
            send(dict(event='chat', scope=scope, message=message, **players[index]))

        def expect(message, reply, index=0):
            before = len(output.read_text(encoding='cp1252'))
            chat(message, index)
            wait(lambda: reply in output.read_text(encoding='cp1252')[before:])
            return output.read_text(encoding='cp1252')[before:]

        def query(sql, args=()):
            with sqlite3.connect(work / 'brenbot.dat') as db:
                return db.execute(sql, args).fetchall()

        try:
            wait(lambda: 'BRenBot ready:' in ''.join(log))
            assert 'jukebox + ping + custom_commands + htmloutput + fds_status_reporter' in ''.join(log)
            sounds = json.loads((work / 'funsounds.json').read_text())
            assert len(sounds) == 169 and sounds['hilfe'] == 'beep.wav'
            assert len(list((root / 'backups').glob('funsounds-*/funsounds.json'))) == 1
            send(dict(event='players', players=players))
            chat('!version')
            wait(lambda: 'BRenBot 1.56' in output.read_text(encoding='cp1252'))
            chat('!help')
            wait(lambda: 'Coop: !buy' in output.read_text(encoding='cp1252'))
            assert output.read_text(encoding='cp1252').count('Coop: !buy') == 1
            chat('!sounds')
            wait(lambda: 'Sounds 1/' in output.read_text(encoding='cp1252'))
            chat('!sounds off', 1)
            wait(lambda: 'sounds off for you' in output.read_text(encoding='cp1252'))
            before = len(output.read_text(encoding='cp1252'))
            chat('!taunt haha', scope='team')
            wait(lambda: 'sound 11 1 laugh1.wav' in output.read_text(encoding='cp1252')[before:])
            emitted = output.read_text(encoding='cp1252')[before:]
            assert 'sound 22 2 ' not in emitted and 'sound 33 3 ' not in emitted
            chat('!taunt haha', scope='team')
            wait(lambda: 'cooldown active' in output.read_text(encoding='cp1252')[before:])
            time.sleep(1.1)
            send(dict(event='leave', **players[1]))
            players[1]['session'] = 24
            send(dict(event='join', **players[1]))
            before = len(output.read_text(encoding='cp1252'))
            chat('well played gg')
            wait(lambda: 'sound 24 2 ' in output.read_text(encoding='cp1252')[before:])
            assert 'sound 33 3 ' in output.read_text(encoding='cp1252')[before:]
            time.sleep(1.1)
            expect('!taunt hilfe', 'sound 11 1 beep.wav')
            time.sleep(1.1)
            expect('!taunt yeah', 'sound 11 1 yeah1.wav')
            time.sleep(1.1)
            before = len(output.read_text(encoding='cp1252'))
            chat('größermuhkleiner')
            time.sleep(.4)
            assert 'sound ' not in output.read_text(encoding='cp1252')[before:]
            expect('muh!', 'sound 11 1 amb_cow1.wav')
            # Jukebox is opt-in, per connection, and survives respawns/maps.
            expect('!tracks', 'Tracks 1/4:')
            before = len(output.read_text(encoding='cp1252'))
            expect('!settrack 0', 'music 11 1 ammoclip.mp3')
            assert 'music 24 2 ' not in output.read_text(encoding='cp1252')[before:]
            expect('!nexttrack', 'Jukebox cooldown active.')
            wait(lambda: output.read_text(encoding='cp1252')[before:].count('music 11 1 ') >= 2, seconds=6)
            time.sleep(.4)
            expect('!settrack 1', 'music 11 1 command&conquer.mp3')
            before = len(output.read_text(encoding='cp1252'))
            players[0]['object'] = 2001
            send(dict(event='players', players=players))
            wait(lambda: 'music 11 1 command&conquer.mp3' in output.read_text(encoding='cp1252')[before:])
            before = len(output.read_text(encoding='cp1252'))
            send(dict(event='map', map='M02.mix'))
            wait(lambda: 'music 11 1 command&conquer.mp3' in output.read_text(encoding='cp1252')[before:])
            time.sleep(.4)
            expect('!settrack level', 'Track not found or ambiguous.')
            expect('!music off', 'music 11 1 off')
            before = len(output.read_text(encoding='cp1252'))
            send(dict(event='leave', **players[0]))
            players[0]['session'] = 13
            send(dict(event='join', **players[0]))
            send(dict(event='players', players=players))
            time.sleep(.4)
            assert 'music 13 1 ' not in output.read_text(encoding='cp1252')[before:]
            expect('!ping', 'Your ping is currently 42')
            ping_reply = expect('!ping Teammate', 'Teammate has a ping of 55.')
            assert 'page 13 1 ' in ping_reply and 'page 24 2 ' not in ping_reply
            expect('!ping NobodyHere', 'NobodyHere was not found in the server.')
            expect('!website', 'https://github.com/Draakoor/renegade-campaign-coop')
            before = len(output.read_text(encoding='cp1252'))
            chat('<script>alert("x")</script>')
            chat('team-log-secret', scope='team')
            chat('!giveweapon me Weapon_RamjetRifle_Player')
            chat('!giveammo all')
            wait(lambda: (work / 'fdsstatus.txt').exists() and 'M02.mix' in (work / 'fdsstatus.txt').read_text())
            status = (work / 'fdsstatus.txt').read_text()
            assert status.splitlines()[:3] == ['Coop', 'M02.mix', '2']
            assert 'Draakoor\tGDI\t0\t0\t0\t42\t' in status
            def html_logs():
                return ''.join(p.read_text() for p in (work / 'HTML_Game_Logs').glob('*.html'))
            wait(lambda: '&lt;script&gt;' in html_logs())
            html = html_logs()
            assert '&lt;script&gt;alert(&quot;x&quot;)&lt;/script&gt;' in html
            assert '<script>' not in html and 'team-log-secret' not in html and '!giveammo' not in html
            # No duplicate/game admin handlers, no command or newline injection.
            before = len(output.read_text(encoding='cp1252'))
            chat('!givecredits me 1000')
            chat('!map M09')
            chat('!taunt haha\nquit')
            chat('a word containing giggle')
            time.sleep(.4)
            assert 'sound ' not in output.read_text(encoding='cp1252')[before:]
            assert not any(x.startswith(('quit', 'kick', 'gameover', 'campaign_admin')) for x in output.read_text(encoding='cp1252').splitlines())
            # Run the real upstream social plugins with the coop patches.
            expect('!setjoin Ready for coop!', 'Your join message has been set.')
            expect('!viewjoin', 'Ready for coop!')
            assert query('SELECT message FROM join_messages WHERE name=?', ('draakoor',)) == [('Ready for coop!',)]
            before = len(output.read_text(encoding='cp1252'))
            send(dict(event='leave', **players[0]))
            players[0]['session'] = 12
            send(dict(event='join', **players[0]))
            send(dict(event='players', players=players))
            send(dict(event='players', players=players))
            wait(lambda: output.read_text(encoding='cp1252')[before:].count('[Draakoor] Ready for coop!') == 3)
            time.sleep(.3)
            assert output.read_text(encoding='cp1252')[before:].count('[Draakoor] Ready for coop!') == 3
            # A delayed disconnect from an earlier session cannot remove a new one.
            send(dict(event='leave', **dict(players[0], session=11)))
            expect('!seen Draakoor', 'is right with you on this server!', 1)
            expect('!seen', 'Usage: !seen <playername>')
            expect('!seen NobodyHere', 'I do not recall seeing NobodyHere')
            players.append(dict(id=4, session=44, name="O'Brien", team=1))
            send(dict(event='join', **players[3]))
            send(dict(event='leave', **players[3]))
            expect("!seen O'Brien", "o'brien was last seen on this server")
            assert query('SELECT name FROM seen WHERE name=?', ("o'brien",)) == [("o'brien",)]
            expect('!seen %', 'I do not recall seeing %')
            send(dict(event='join', **players[3]))
            expect('!rec', 'Usage: !recommend')
            expect('!rec Draakoor teamwork', 'not allowed to recommend yourself')
            expect("!rec Teammate Great teamwork", 'has been recommended by Draakoor')
            expect('!rec Teammate Again', 'already recommended Teammate today')
            expect('!recs', 'You currently have 1 recommendations.', 1)
            listing = expect('!teamplayers', 'Teamplayers based on recommendations')
            wait(lambda: 'Teammate (1)' in output.read_text(encoding='cp1252'))
            assert 'None of the players' not in listing
            expect('!rec 4 Thanks for helping', 'has been recommended by Draakoor')
            assert query('SELECT recommendations FROM recommendations WHERE name=?', ("O'Brien",)) == [(1,)]
            expect('!rec 3 Thanks', 'has been recommended by Draakoor')
            for n in range(5, 8):
                players.append(dict(id=n, session=n*11, name=f'CoopTest{n}', team=1))
                send(dict(event='join', **players[-1]))
            expect('!rec 5 Thanks', 'has been recommended by Draakoor')
            expect('!rec 6 Thanks', 'has been recommended by Draakoor')
            expect('!rec 7 Thanks', 'daily limit')
            before = len(output.read_text(encoding='cp1252'))
            chat('!n00b Teammate test')
            chat('!recignore Teammate')
            chat('!setjoin injected\nquit')
            time.sleep(.4)
            assert not query('SELECT name FROM recommendations_ignored_users')
            assert query('SELECT SUM(n00bs) FROM recommendations') == [(0,)]
            assert query('SELECT message FROM join_messages WHERE name=?', ('draakoor',)) == [('Ready for coop!',)]
            assert len(list((root / 'backups').glob('brenbot-social-*/brenbot.dat'))) == 1
            # Bot restart reuses state and never replays old chat/recommendations.
            stop(proc)
            thread.join(timeout=2)
            # A removed extra must stay removed; operator overrides remain intact.
            sounds.pop('yeah')
            (work / 'funsounds.json').write_text(json.dumps(sounds))
            before = len(output.read_text(encoding='cp1252'))
            prepare(root, core)
            proc = subprocess.Popen(['perl', str(core / 'brenbot.pl')], cwd=work, env=env,
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            old_ready = ''.join(log).count('BRenBot ready:')
            thread = threading.Thread(target=lambda: log.extend(iter(proc.stdout.readline, '')), daemon=True)
            thread.start()
            wait(lambda: ''.join(log).count('BRenBot ready:') > old_ready)
            assert json.loads((work / 'funsounds.json').read_text()) == sounds
            assert len(list((root / 'backups').glob('funsounds-*/funsounds.json'))) == 1
            send(dict(event='players', players=players))
            expect('!viewjoin', 'Ready for coop!')
            expect('!recs', 'You currently have 1 recommendations.', 1)
            assert 'has been recommended' not in output.read_text(encoding='cp1252')[before:]
            assert len(list((root / 'backups').glob('brenbot-social-*/brenbot.dat'))) == 1
            expect('!setjoin off', 'Your join message has been removed.')
            expect('!viewjoin', 'do not have a join message')
            assert 'TEST: BRenBot attempted' not in ''.join(log)
            assert 'Error' not in ''.join(log), ''.join(log)
            assert 'failed' not in ''.join(log).lower(), ''.join(log)
            assert 'Unloading plugin' not in ''.join(log), ''.join(log)
            # Re-running setup preserves operator configuration and bot state.
            custom = (work / 'funsounds.json').read_bytes()
            prepare(root, core)
            assert (work / 'funsounds.json').read_bytes() == custom
            print('PASS: real BRenBot, ten plugins, sound migration/Unicode/mute, personal jukebox/respawn/map/reconnect, ping, private escaped logs/status, social quotas/persistence, backups, command isolation, no network.')
        finally:
            stop(proc)
            thread.join(timeout=2)
            print(''.join(log))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--core', type=Path, required=True)
    test(parser.parse_args().core.resolve())
