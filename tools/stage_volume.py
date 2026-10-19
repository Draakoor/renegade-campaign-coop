"""Assemble game assets in the private persistent volume, outside the image."""
import json
import os
from pathlib import Path
import shutil
from urllib.parse import urlsplit
from .fetch_inputs import fetch, safe_extract, sha, PROJECT
from .prepare_bundle import prepare


def check_inputs(root):
    root = Path(root)
    lock = json.loads((PROJECT / 'sources.lock.json').read_text())
    inputs = {'coopbeta': root / 'input' / 'CoopBeta424.zip', 'fds_data': root / 'input' / 'server.dat'}
    for name, path in inputs.items():
        if not path.is_file():
            raise RuntimeError(f'Missing {path}. Supply CoopBeta424.zip and the original FDS server.dat in input/; see README.')
        if sha(path) != lock[name]['sha256']:
            raise RuntimeError(f'{path.name}: input hash mismatch')
    url = os.environ.get('TT_DOWNLOAD_URL', '')
    parsed = urlsplit(url)
    if parsed.scheme not in ('http', 'https') or not parsed.hostname or parsed.username or parsed.password or parsed.query or parsed.fragment:
        raise RuntimeError('Set TT_DOWNLOAD_URL to this server public HTTP(S) download URL, e.g. http://YOUR_IP:35002/')
    return inputs, url


def stage_volume(root):
    root = Path(root).resolve()
    if (root / 'release').exists() or (root / 'server').exists():
        raise RuntimeError('Incomplete existing volume; preserve it and repair the staged release before starting.')
    inputs, url = check_inputs(root)
    plugin = PROJECT / 'artifacts' / 'da_campaign.dll'
    if not plugin.is_file():
        raise RuntimeError('Compiled plugin missing. Use the published image or build artifacts/da_campaign.dll.')
    work = root / '.cache' / 'campaign-build'
    work.mkdir(parents=True, exist_ok=True)
    for name in ('tt', 'dragonade'):
        fetch(work, name)
    shutil.copyfile(inputs['coopbeta'], work / 'downloads' / 'CoopBeta424.zip')
    safe_extract(inputs['coopbeta'], work / 'legacy')
    (work / 'build').mkdir(exist_ok=True)
    shutil.copyfile(plugin, work / 'build' / 'da_campaign.dll')
    import tempfile
    # A failed build remains isolated; the final release appears only when complete.
    with tempfile.TemporaryDirectory(prefix='assemble-', dir=work) as tmp:
        out = Path(tmp) / 'bundle'
        prepare(work, None, out, fds_data=inputs['fds_data'], download_url=url, archive=False)
        for name in ('defaults', 'source'):
            if not (root / name).exists():
                shutil.copytree(out / name, root / name)
        if not (root / 'validation.json').exists():
            shutil.copyfile(out / 'validation.json', root / 'validation.json')
        (out / 'release').rename(root / 'release')
    print('[Installer] Private campaign assets staged and checksummed.', flush=True)
