"""Explicit operator opt-in to public testing, with recoverable configuration backups."""
import argparse
import configparser
from datetime import datetime, timezone
import io
import json
import os
from pathlib import Path
import shutil
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from volume_lock import volume_lock


def configure(root, mode, lock_held=False):
    root = Path(root).resolve()
    if not lock_held:
        with volume_lock(root):
            return configure(root, mode, lock_held=True)
    if mode not in ('development', 'public-test'):
        raise ValueError('Unknown publication mode')
    files = ('server/data/svrcfg_cnc.ini', 'server/da.ini')
    settings = []
    for name in files:
        cfg = configparser.ConfigParser(interpolation=None)
        cfg.optionxform = str
        with (root / name).open(encoding='utf-8') as source:
            cfg.read_file(source)
        settings.append(cfg)
    game, da = settings
    if not game.get('Settings', 'bPassword', fallback=''):
        raise RuntimeError('Keep the existing private test password for reverting to development mode.')
    game.set('Settings', 'IsPassworded', 'no' if mode == 'public-test' else 'yes')
    game.set('Settings', 'bMotd', 'Public campaign test - mission playtests pending' if mode == 'public-test'
             else 'Campaign development server - validation pending')
    da.set('General', 'EnableGameSpy', '1')
    da.set('General', 'GameSpyMasterServers', '127.0.0.1')
    da.set('General', 'GameSpyQueryPort', '35001')
    da.set('General', 'CampaignDiagnostics', '0')
    if not da.has_section('GameSpyRules'):
        da.add_section('GameSpyRules')
    da.set('GameSpyRules', 'Publication', mode)
    da.set('GameSpyRules', 'Validation', 'pending')
    payloads = {}
    for name, cfg in zip(files, settings):
        out = io.StringIO()
        cfg.write(out, space_around_delimiters=False)
        payloads[name] = out.getvalue()
    payloads['publication.json'] = json.dumps({'mode': mode}, indent=2) + '\n'
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    backup = root / 'backups' / ('publication-' + stamp)
    backup.mkdir(parents=True)
    for name in payloads:
        source = root / name
        if source.exists():
            target = backup / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)
    try:
        for name, data in payloads.items():
            target = root / name
            temporary = target.with_name(target.name + '.publication-tmp')
            temporary.write_text(data, encoding='utf-8')
            os.replace(temporary, target)
    except OSError:
        for name in payloads:
            saved = backup / name
            target = root / name
            if saved.exists():
                shutil.copy2(saved, target)
            elif name == 'publication.json':
                target.unlink(missing_ok=True)
        raise
    print(f'[Publication] {mode}; previous configuration saved in {backup}.', flush=True)
    return backup


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--mode', choices=('development', 'public-test'), required=True)
    args = parser.parse_args()
    configure(args.root, args.mode)
