"""Fetch pinned public toolchain inputs; never download credentials or live data."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import urllib.request
import zipfile

PROJECT = Path(__file__).resolve().parents[1]


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def safe_extract(archive, destination):
    destination = Path(destination).resolve()
    with zipfile.ZipFile(archive) as zipped:
        for info in zipped.infolist():
            target = (destination / info.filename).resolve()
            if not target.is_relative_to(destination) or '\\' in info.filename:
                raise ValueError('Archive contains an unsafe path')
            if (info.external_attr >> 16) & 0o170000 == 0o120000:
                raise ValueError('Archive symlinks are not accepted')
        zipped.extractall(destination)


def fetch(work, name):
    lock = json.loads((PROJECT / 'sources.lock.json').read_text())
    entry = lock[name]
    filename = {'tt': 'server-4.8.4.zip', 'dragonade': 'dragonade-1.11.1.zip'}[name]
    archive = Path(work) / 'downloads' / filename
    archive.parent.mkdir(parents=True, exist_ok=True)
    if not archive.exists():
        temp = archive.with_suffix('.downloading')
        request = urllib.request.Request(entry['url'], headers={'User-Agent': 'Novara-Renegade-Coop/0.1'})
        with urllib.request.urlopen(request, timeout=120) as response, temp.open('wb') as output:
            shutil.copyfileobj(response, output)
        if sha(temp) != entry['sha256']:
            raise RuntimeError(f'{name}: download hash mismatch')
        temp.replace(archive)
    if sha(archive) != entry['sha256']:
        raise RuntimeError(f'{name}: input hash mismatch')
    destination = Path(work) / ('tt' if name == 'tt' else 'da')
    safe_extract(archive, destination)
    return archive


if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('--work', type=Path, required=True)
    p.add_argument('--only', choices=['tt', 'dragonade'])
    args = p.parse_args()
    for name in ([args.only] if args.only else ['tt', 'dragonade']):
        print(fetch(args.work, name))
