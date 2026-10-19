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
    args = parser.parse_args()
    if not re.fullmatch('[0-9a-f]{64}', args.from_sha256):
        raise ValueError('Expected the currently deployed plugin SHA256')
    root = Path(__file__).resolve().parents[1]
    files = {'server/da_campaign.dll': args.plugin, 'release/server/da_campaign.dll': args.plugin}
    for folder in ('plugin', 'tools', 'validation'):
        for path in (root / folder).iterdir():
            if path.is_file() and path.suffix in ('.h', '.cpp', '.py', '.json'):
                files['source/' + folder + '/' + path.name] = path
    for name in ('README.md', 'COPYING', 'COPYING.plugin', 'THIRD_PARTY.txt'):
        files['source/' + name] = root / name
    files['defaults/da.ini'] = root / 'defaults/da.ini'
    config = configparser.ConfigParser()
    config.optionxform = str
    config.read(root / 'defaults/da.ini')
    metadata = {'from_sha256': args.from_sha256,
                'files': {name: digest(path.read_bytes()) for name, path in files.items()},
                'settings': {key: value for key, value in config['General'].items() if key.startswith('Campaign')}}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(args.output, 'w', zipfile.ZIP_DEFLATED) as archive:
        archive.writestr('update.json', json.dumps(metadata, indent=2))
        for name, path in files.items():
            archive.write(path, 'files/' + name)
    print('Plugin SHA256: ' + metadata['files']['server/da_campaign.dll'])


if __name__ == '__main__':
    main()
