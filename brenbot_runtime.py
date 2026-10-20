"""Persistent BRenBot configuration; immutable core comes from the image."""
from datetime import datetime, timezone
import os
from pathlib import Path
import re
import shutil
import subprocess


def prepare(root, core=Path('/opt/brenbot')):
    if not (core / 'NovaraLocal.pm').is_file():
        raise RuntimeError('This image does not include the Novara BRenBot runtime.')
    target = root / 'brenbot'
    target.mkdir(exist_ok=True)
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')

    def put(relative, data, managed=False):
        path = target / relative
        if path.exists():
            if not managed or path.read_bytes() == data:
                return
            backup = root / 'backups' / ('brenbot-' + stamp) / relative
            backup.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, backup)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)

    for file in core.iterdir():
        if file.suffix in ('.cfg', '.xml') or file.name in ('funsounds.json', 'UPSTREAM.json'):
            if file.name in ('brenbot.cfg', 'moderators.cfg'):
                continue
            put(Path(file.name), file.read_bytes(), file.name in ('commands.xml', 'UPSTREAM.json'))
    for file in (core / 'plugins').iterdir():
        put(Path('plugins') / file.name, file.read_bytes(), file.suffix == '.pm')
    cfg = (core / 'brenbot.cfg').read_text()
    values = {
        'IrcServer': '127.0.0.1', 'IrcPort': '6667', 'BotName': 'NovaraCoop',
        'ircAdminChannel': '#disabled', 'ircPublicChannel': '',
        'FDSLogFilePath': str(root / 'server') + '/',
        'FDSConfigFile': str(root / 'server/data/svrcfg_cnc.ini'),
        'VotingEnabled': '0', 'VehicleKick': '0',
        'Moderators_Show_Symbols': '0', 'Moderators_Show_Join_Message': '0',
        'Gamelog_Show_Vehicle_Stolen': '0',
    }
    for key, value in values.items():
        cfg = re.sub(r'^' + re.escape(key) + r'\s*=.*$', lambda _: key + ' = ' + value, cfg, flags=re.MULTILINE)
    put(Path('brenbot.cfg'), cfg.encode())
    # Nickname-only moderators are intentionally not imported.
    put(Path('moderators.cfg'), b'[ADMINS]\n[HALF_MODS]\n[FULL_MODS]\n')
    return target


def start(root, core=Path('/opt/brenbot')):
    target = prepare(root, core)
    env = os.environ.copy()
    env['NOVARA_SERVER'] = str(root / 'server')
    env['PERL5LIB'] = str(core)
    return subprocess.Popen(['perl', str(core / 'brenbot.pl')], cwd=target, env=env,
                            stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, encoding='utf-8', errors='replace')


def stop(bot):
    if bot is None:
        return
    if bot.poll() is None:
        bot.terminate()
        try:
            bot.wait(timeout=5)
        except subprocess.TimeoutExpired:
            bot.kill()
            bot.wait(timeout=5)
    if bot.stdout:
        bot.stdout.close()
