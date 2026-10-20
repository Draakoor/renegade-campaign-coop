"""Fetch pinned upstream BRenBot and add the local, network-free adapter."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import urllib.request
import xml.etree.ElementTree as ET
import zipfile
import io

REPO = Path(__file__).resolve().parents[1]


def build(output, archive=None):
    lock = json.loads((REPO / 'sources.lock.json').read_text())['brenbot']
    data = archive.read_bytes() if archive else urllib.request.urlopen(lock['url'], timeout=90).read()
    if hashlib.sha256(data).hexdigest() != lock['sha256']:
        raise ValueError('BRenBot archive checksum mismatch')
    output.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(io.BytesIO(data)) as bundle:
        roots = {name.split('/')[0] for name in bundle.namelist()}
        if len(roots) != 1:
            raise ValueError('Unexpected BRenBot archive layout')
        root = next(iter(roots))
        for member in bundle.namelist():
            prefix = root + '/Source/'
            if not member.startswith(prefix) or member.endswith('/'):
                continue
            relative = Path(member[len(prefix):])
            if relative.is_absolute() or '..' in relative.parts:
                raise ValueError('Unsafe BRenBot archive path')
            # Only source/configuration, never obsolete bundled executables.
            if relative.suffix.lower() not in ('.pm', '.pl', '.cfg', '.xml', '.txt'):
                continue
            target = output / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(bundle.read(member))
        upstream = bundle.read(root + '/Plugins/funsounds/funsounds.pm').decode()
        keywords = dict(re.findall(r'^\s*"([^"\n]+)"\s*=>\s*"([^"\n]+\.wav)"', upstream, re.MULTILINE))
        if len(keywords) != 49:
            raise ValueError('Unexpected upstream Funsounds keyword catalog')
        (output / 'funsounds.json').write_text(json.dumps(keywords, indent=2) + '\n')
    main = output / 'brenbot.pl'
    content = main.read_text()
    content = content.replace('brdatabase::db_connect();', 'use NovaraLocal;\nNovaraLocal::install();\nbrdatabase::db_connect();', 1)
    content = content.replace('brIRC::init();', 'NovaraLocal::start();', 1)
    main.write_text(content)
    commands = output / 'commands.pm'
    commands.write_text(commands.read_text().replace("ForceArray => [ 'group' ]", "ForceArray => [ 'group', 'command' ]", 1))
    config = output / 'brconfig.pm'
    content, count = re.subn(r'  if \( \(\$\^O ne "MSWin32"\) \)\n  \{.*?does not support the LFDS.*?\n  \}',
                            '  # Novara: Linux Perl talks to the Windows FDS under Wine through local files.',
                            config.read_text(), count=1, flags=re.DOTALL)
    if count != 1:
        raise ValueError('Pinned BRenBot OS guard changed')
    config.write_text(content)
    # Unified help and gameplay commands have exactly one owner. Retain the
    # upstream version implementation; its privileged commands are absent.
    tree = ET.parse(output / 'commands.xml')
    for item in list(tree.getroot()):
        if item.tag == 'command' and item.get('name') != 'version':
            tree.getroot().remove(item)
    tree.write(output / 'commands.xml', encoding='utf-8', xml_declaration=True)
    shutil.copy2(REPO / 'brenbot/NovaraLocal.pm', output / 'NovaraLocal.pm')
    shutil.copytree(REPO / 'brenbot/plugins', output / 'plugins', dirs_exist_ok=True)
    (output / 'UPSTREAM.json').write_text(json.dumps(lock, indent=2) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--archive', type=Path)
    args = parser.parse_args()
    build(args.output, args.archive)
