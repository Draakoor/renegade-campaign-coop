"""Package a compiled plugin, public source and missing campaign defaults."""
import argparse
import configparser
import json
from pathlib import Path
import re
import zipfile

from update_plugin import digest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--plugin', type=Path, required=True)
    parser.add_argument('--from-sha256', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--brenbot-migration', action='store_true', help='Update the supervisor and select M01-M11; requires the new BRenBot image')
    args = parser.parse_args()
    if not re.fullmatch('[0-9a-f]{64}', args.from_sha256):
        raise ValueError('Expected the currently deployed plugin SHA256')
    root = Path(__file__).resolve().parents[1]
    files = {'server/da_campaign.dll': args.plugin, 'release/server/da_campaign.dll': args.plugin}
    for folder in ('plugin', 'tools', 'validation'):
        for path in (root / folder).iterdir():
            if path.is_file() and path.suffix in ('.h', '.cpp', '.py', '.json'):
                files['source/' + folder + '/' + path.name] = path
    for name in ('README.md', 'COPYING', 'COPYING.plugin', 'THIRD_PARTY.txt', 'sources.lock.json'):
        files['source/' + name] = root / name
    files['defaults/da.ini'] = root / 'defaults/da.ini'
    for path in (root / 'brenbot').rglob('*'):
        if path.is_file():
            files['source/' + path.relative_to(root).as_posix()] = path
    if args.brenbot_migration:
        for name in ('runtime.py', 'brenbot_runtime.py'):
            files[name] = root / name
    config = configparser.ConfigParser()
    config.optionxform = str
    config.read(root / 'defaults/da.ini')
    metadata = {'from_sha256': args.from_sha256,
                'files': {name: digest(path.read_bytes()) for name, path in files.items()},
                'settings': {key: value for key, value in config['General'].items() if key.startswith('Campaign')}}
    if args.brenbot_migration:
        metadata['migration'] = 'novara-brenbot-m01-v1'
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(args.output, 'w', zipfile.ZIP_DEFLATED) as archive:
        archive.writestr('update.json', json.dumps(metadata, indent=2))
        for name, path in files.items():
            archive.write(path, 'files/' + name)
    print('Plugin SHA256: ' + metadata['files']['server/da_campaign.dll'])


if __name__ == '__main__':
    main()
