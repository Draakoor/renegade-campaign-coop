"""Install a verified plugin update in a stopped volume, preserving settings."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import runpy
import shutil
import sys
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from volume_lock import volume_lock


def digest(data):
    return hashlib.sha256(data).hexdigest()


def add_setting(text, key, value):
    match = re.search(r'(?ims)^\[General\][^\n]*\n(.*?)(?=^\[|\Z)', text)
    if not match:
        return text.rstrip() + '\n\n[General]\n' + key + '=' + value + '\n'
    body = match.group(1)
    if re.search(r'(?mi)^[ \t]*' + re.escape(key) + r'\s*=', body):
        return text
    body = body.rstrip() + '\n' + key + '=' + value + '\n\n'
    return text[:match.start(1)] + body + text[match.end(1):]


def target(root, name):
    path = PurePosixPath(name)
    if path.is_absolute() or '..' in path.parts or '\\' in name or ':' in name:
        raise ValueError('Invalid update path')
    result = root.joinpath(*path.parts)
    if not result.resolve().is_relative_to(root):
        raise ValueError('Update path escapes volume')
    return result


def migrate_campaign(root, targets):
    """Explicit operator-requested migration; preserve unrelated configuration."""
    maps = [f'M{i:02d}' for i in range(1, 12)]
    for name in ('server/tt.cfg', 'defaults/tt.cfg'):
        if not target(root, name).exists():
            if name.startswith('defaults/'):
                continue
            raise ValueError('Missing live tt.cfg')
        text = target(root, name).read_text(encoding='utf-8-sig')
        text, count = re.subn(r'^\s*rotation\s*:\s*\[[^\]]*\]\s*;',
                              'rotation: ' + json.dumps(maps) + ';', text, flags=re.MULTILINE)
        if count != 1:
            raise ValueError('Expected exactly one explicit rotation in ' + name)
        targets[name] = text.encode()
    for name in ('server/data/svrcfg_cnc.ini', 'defaults/data/svrcfg_cnc.ini'):
        if not target(root, name).exists():
            if name.startswith('defaults/'):
                continue
            raise ValueError('Missing live svrcfg_cnc.ini')
        text = target(root, name).read_text(encoding='utf-8-sig')
        text = re.sub(r'(?mi)^MapName\d*\s*=.*\n?', '', text)
        section = re.search(r'(?ims)^\[Settings\][^\n]*\n(.*?)(?=^\[|\Z)', text)
        if not section:
            raise ValueError('Missing Settings section in ' + name)
        rotation = 'MapName=M01.mix\n' + ''.join(f'MapName{i:02d}={map}.mix\n' for i, map in enumerate(maps))
        text = text[:section.end(1)] + rotation + text[section.end(1):]
        targets[name] = text.encode()
    text = targets['server/da.ini'].decode()
    obsolete = ('CampaignAdaptiveDifficulty', 'CampaignEnemyResistancePerPlayer', 'CampaignEnemyDamagePerPlayer',
                'CampaignDifficultyUpdateSeconds', 'CampaignTerminalModel', 'CampaignTerminalSpacing')
    for key in obsolete:
        text = re.sub(r'(?mi)^\s*' + key + r'\s*=.*\n?', '', text)
    for key in ('EnableChatSounds', 'EnableTaunts', 'EnableDonate'):
        text = re.sub(r'(?mi)^\s*' + key + r'\s*=.*\n?', '', text)
        text = add_setting(text, key, '0')
    targets['server/da.ini'] = text.encode()


def apply(root, payload):
    root = root.resolve()
    with volume_lock(root):
        if (root / 'runtime.lock').exists():
            raise RuntimeError('Investigate the legacy runtime lock before updating')
        with zipfile.ZipFile(payload) as archive:
            metadata = json.loads(archive.read('update.json'))
            migration = metadata.get('migration')
            if migration not in (None, 'novara-brenbot-m01-v1'):
                raise ValueError('Unsupported migration')
            targets = {}
            for name, expected in metadata['files'].items():
                target(root, name)
                runtime_file = migration == 'novara-brenbot-m01-v1' and name in ('runtime.py', 'brenbot_runtime.py')
                if not runtime_file and name not in ('server/da_campaign.dll', 'release/server/da_campaign.dll') and not name.startswith(('source/', 'defaults/')):
                    raise ValueError('Unexpected update target: ' + name)
                data = archive.read('files/' + name)
                if digest(data) != expected:
                    raise ValueError('Payload checksum mismatch: ' + name)
                targets[name] = data
        binary = targets['server/da_campaign.dll']
        if not binary.startswith(b'MZ') or targets['release/server/da_campaign.dll'] != binary:
            raise ValueError('Invalid or inconsistent plugin binaries')
        new = digest(binary)
        allowed = (metadata['from_sha256'], new)
        for name in ('server/da_campaign.dll', 'release/server/da_campaign.dll'):
            if digest(target(root, name).read_bytes()) not in allowed:
                raise ValueError('Unexpected installed plugin: ' + name)
        for name in ('installed.json', 'release/manifest.json'):
            manifest = json.loads(target(root, name).read_text())
            if manifest['files']['da_campaign.dll'] not in allowed:
                raise ValueError('Unexpected manifest plugin: ' + name)
            manifest['files']['da_campaign.dll'] = new
            targets[name] = (json.dumps(manifest, indent=2) + '\n').encode()
        text = target(root, 'server/da.ini').read_text(encoding='utf-8-sig')
        for key, value in metadata.get('settings', {}).items():
            if not re.fullmatch(r'Campaign[A-Za-z0-9]+', key) or '\n' in value or '\r' in value:
                raise ValueError('Invalid campaign setting')
            text = add_setting(text, key, value)
        targets['server/da.ini'] = text.encode('utf-8')
        if migration:
            if not all(name in targets for name in ('runtime.py', 'brenbot_runtime.py')):
                raise ValueError('Incomplete BRenBot supervisor migration')
            migrate_campaign(root, targets)
        changed = {name: data for name, data in targets.items()
                   if not target(root, name).exists() or target(root, name).read_bytes() != data}
        if not changed:
            print('[Novara update] Already applied.', flush=True)
            return None
        backup = root / 'backups' / ('coop-features-' + datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ'))
        backup.mkdir(parents=True)
        absent = []
        for name in changed:
            src = target(root, name)
            if src.exists():
                dst = backup / name
                dst.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(src, dst)
            else:
                absent.append(name)
        (backup / 'transaction.json').write_text(json.dumps({'files': list(changed), 'previously_absent': absent}, indent=2))
        written = []
        try:
            for name, data in changed.items():
                dst = target(root, name)
                dst.parent.mkdir(parents=True, exist_ok=True)
                temporary = dst.with_name(dst.name + '.coop-update.tmp')
                temporary.write_bytes(data)
                os.replace(temporary, dst)
                written.append(name)
            for name, data in changed.items():
                if target(root, name).read_bytes() != data:
                    raise RuntimeError('Written file mismatch: ' + name)
        except BaseException:
            for name in reversed(written):
                dst = target(root, name)
                if (backup / name).exists():
                    shutil.copy2(backup / name, dst)
                else:
                    dst.unlink()
            raise
        print('[Novara update] Applied and verified. Backup: ' + str(backup), flush=True)
        print('[Novara update] Plugin SHA256: ' + new, flush=True)
        return backup


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--payload', type=Path, required=True)
    parser.add_argument('--start', action='store_true')
    args = parser.parse_args()
    apply(args.root, args.payload)
    if args.start:
        runtime = args.root.resolve() / 'runtime.py'
        sys.argv = [str(runtime)]
        runpy.run_path(str(runtime), run_name='__main__')
