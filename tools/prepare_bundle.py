"""Assemble the private campaign, including the M06/M07/M09 runtime adaptations."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import zipfile
if __package__:
    from .fds_port import set_lan_port
    from .mission_sources import compile_source
else:
    from fds_port import set_lan_port
    from mission_sources import compile_source

PROJECT = Path(__file__).resolve().parents[1]
MAPS = [f'M{i:02d}' for i in range(1, 12)]
EXISTING = [m for m in MAPS if m not in ('M06', 'M07', 'M09')]
ADAPTED = ('M06', 'M07', 'M09')


def digest(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def copy_tree(source, dest, exclude=()):
    for src in source.rglob('*'):
        if src.is_file() and src.suffix.lower() not in exclude:
            dst = dest / src.relative_to(source)
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(src, dst)


def prepare(work, game, output, *, fds_data=None, mission_input=None, download_url='http://127.0.0.1:35002/', archive=True):
    work, output = work.resolve(), output.resolve()
    game = game.resolve() if game else None
    if output.exists():
        raise RuntimeError('Use a new output directory; existing builds are retained.')
    lock = json.loads((PROJECT / 'sources.lock.json').read_text())
    mission_files = {}
    for name in ADAPTED:
        for suffix, folder in [('mix', 'Data'), ('LVL', 'Modding Support/Single Player Level Source Files')]:
            src = Path(mission_input) / (name + '.' + suffix) if mission_input else game / folder / (name + '.' + suffix)
            if digest(src) != lock['campaign_missions'][name][suffix.lower() + '_sha256']:
                raise RuntimeError(f'{name}.{suffix}: original mission input hash mismatch')
            mission_files[name, suffix] = src
    for key, filename in [('tt', 'server-4.8.4.zip'), ('dragonade', 'dragonade-1.11.1.zip')]:
        if digest(work / 'downloads' / filename) != lock[key]['sha256']:
            raise RuntimeError(f'{key} input hash mismatch')
    coop_archive = work / 'downloads' / 'CoopBeta424.zip'
    if digest(coop_archive) != lock['coopbeta']['sha256']:
        raise RuntimeError('CoopBeta input hash mismatch')
    server = output / 'release' / 'server'
    server.mkdir(parents=True)
    legacy = work / 'legacy' / 'CoopBeta424'
    # Never copy legacy executable/DLL/configuration files into the new runtime.
    for name in ['always.dat', 'always2.dat', 'always.dbs', 'objects.mpf', 'acallow.ini']:
        (server / 'data').mkdir(exist_ok=True)
        shutil.copyfile(legacy / 'data' / name, server / 'data' / name)
    for name in EXISTING + ['CoopContent']:
        shutil.copyfile(legacy / 'data' / 'original mix files' / (name + '.mix'), server / 'data' / (name + '.mix'))
    copy_tree(work / 'tt' / 'serverfiles', server, exclude=('.ini', '.cfg'))
    # TT includes its own M09.mix. Install the verified campaign input after TT,
    # so the adaptation, LVL placements and original map are a matching set.
    for name in ADAPTED:
        shutil.copyfile(mission_files[name, 'mix'], server / 'data' / (name + '.mix'))
        for folder in ('original', 'working'):
            target = work / 'missions' / folder / (name + '.LVL')
            target.parent.mkdir(parents=True, exist_ok=True)
            if not target.exists():
                shutil.copyfile(mission_files[name, 'LVL'], target)
        compile_source(mission_files[name, 'LVL'], server / 'data' / (name + '.wcm'))
    executable = server / 'server.exe'
    executable.write_bytes(set_lan_port(executable.read_bytes()))
    shutil.copyfile(work / 'tt' / 'serverfiles' / 'data' / 'anticheat.ini', server / 'data' / 'anticheat.ini')
    for src in (work / 'da' / 'Server').iterdir():
        if src.is_file() and src.suffix.lower() in ('.dll', '.txt'):
            shutil.copyfile(src, server / src.name)
    shutil.copyfile(work / 'build' / 'da_campaign.dll', server / 'da_campaign.dll')
    # Original FDS data needed by the TT-patched executable.
    original_fds = fds_data or game / 'RenegadeFDS' / 'Server.dat'
    if digest(original_fds) != lock['fds_data']['sha256']:
        raise RuntimeError('Original FDS server.dat hash mismatch')
    shutil.copyfile(original_fds, server / 'server.dat')
    copy_tree(legacy / 'MPFGames' / 'CoopServer' / 'ttfs', server / 'NovaraRenegade' / 'FDS' / 'ttfs')

    defaults = output / 'defaults'
    defaults.mkdir()
    for src in (PROJECT / 'defaults').iterdir():
        dst = defaults / ('data' if src.name in ('svrcfg_cnc.ini', 'paths.ini') else '') / src.name
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(src, dst)
    definitions = 'gameDefinitions:\n{\n'
    for name in EXISTING:
        definitions += f'  {name}: {{ mapName="{name}"; packages=["{name}","CoopContent"]; serverPresetsFile="objects.mpf"; }};\n'
    for name in ADAPTED:
        definitions += f'  {name}: {{ mapName="{name}"; packages=["CoopContent"]; serverPresetsFile="objects.mpf"; }};\n'
    definitions += '};\nrotation: ' + json.dumps(MAPS) + ';\n'
    definitions += 'downloader: { repositoryUrl=' + json.dumps(download_url.rstrip('/') + '/') + '; };\n'
    (defaults / 'tt.cfg').write_text(definitions, encoding='utf-8')
    for name in ('bootstrap.py', 'runtime.py', 'brenbot_runtime.py', 'gamespy.py', 'volume_lock.py', 'sources.lock.json'):
        shutil.copyfile(PROJECT / name, output / name)
    # Ship the matching source and licence with our compiled plugin.
    copy_tree(PROJECT / 'plugin', output / 'source' / 'plugin')
    copy_tree(PROJECT / 'tools', output / 'source' / 'tools', exclude=('.pyc',))
    shutil.copyfile(PROJECT / 'COPYING', output / 'source' / 'COPYING')
    shutil.copyfile(PROJECT / 'COPYING.plugin', output / 'source' / 'COPYING.plugin')
    shutil.copyfile(PROJECT / 'THIRD_PARTY.txt', output / 'source' / 'THIRD_PARTY.txt')
    validation = {'release': 'development', 'desired_rotation': MAPS, 'missions': {},
                  'network': {}, 'load_test': {'result': 'not_run', 'clients': 0}}
    for name in MAPS:
        validation['missions'][name] = {'implementation': 'legacy_unverified' if name in EXISTING else 'runtime_adaptation_unverified',
                                        'clients': 0, 'evidence': []}
    (output / 'validation.json').write_text(json.dumps(validation, indent=2), encoding='utf-8')
    files = {p.relative_to(server).as_posix(): digest(p) for p in sorted(server.rglob('*')) if p.is_file()}
    manifest = {'build': output.name, 'public_ready': False, 'files': files, 'sources': lock}
    (output / 'release' / 'manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
    # Preserve original LVL sources and make separate editable copies.
    authoring = work / 'missions'
    for name in ('M06', 'M07', 'M09'):
        if game is None:
            continue
        src = game / 'Modding Support' / 'Single Player Level Source Files' / (name + '.LVL')
        for folder in ('original', 'working'):
            dst = authoring / folder / src.name
            dst.parent.mkdir(parents=True, exist_ok=True)
            if not dst.exists():
                shutil.copyfile(src, dst)
    if not archive:
        return output
    archive_path = output.with_suffix('.zip')
    with zipfile.ZipFile(archive_path, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as z:
        for path in sorted(output.rglob('*')):
            if path.is_file():
                z.write(path, path.relative_to(output).as_posix())
    print(json.dumps({'archive': str(archive_path), 'bytes': archive_path.stat().st_size, 'sha256': digest(archive_path)}, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--game', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--download-url', required=True, help='Public HTTP(S) URL of this server on port 35002')
    args = parser.parse_args()
    prepare(args.work, args.game, args.output, download_url=args.download_url)
