"""Install the prepared, checksummed release into the persistent server volume.

Existing settings are never overwritten. Changed managed binaries/assets are
backed up before replacement. Run only while the Renegade process is stopped.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import secrets
import shutil
import time
from uuid import uuid4
from volume_lock import volume_lock

ROOT = Path(__file__).resolve().parent


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def install(root=ROOT, lock_held=False):
    if not lock_held:
        with volume_lock(root):
            return install(root, lock_held=True)
    root = Path(root).resolve()
    # Legacy supervisors use this different lock; never install over one.
    if (root / 'runtime.lock').exists():
        raise RuntimeError('Legacy runtime.lock exists; stop the old server before migration.')
    release = root / 'release'
    manifest = json.loads((release / 'manifest.json').read_text(encoding='utf-8'))
    # Validate the entire staged release before changing any live file.
    for name, digest in manifest['files'].items():
        candidate = (release / 'server' / name).resolve()
        if not candidate.is_relative_to((release / 'server').resolve()) or sha(candidate) != digest:
            raise RuntimeError(f'Release verification failed: {name}')
    server = root / 'server'
    backup = root / 'backups' / (time.strftime('%Y%m%d-%H%M%S') + '-' + uuid4().hex[:8])
    for name, digest in manifest['files'].items():
        dst = server / name
        src = release / 'server' / name
        if dst.exists() and sha(dst) == digest:
            continue
        if dst.exists():
            saved = backup / name
            saved.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(dst, saved)
        dst.parent.mkdir(parents=True, exist_ok=True)
        temporary = dst.with_name(dst.name + '.installing')
        shutil.copyfile(src, temporary)
        os.replace(temporary, dst)
    for src in (root / 'defaults').rglob('*'):
        if not src.is_file():
            continue
        dst = server / src.relative_to(root / 'defaults')
        if not dst.exists():
            dst.parent.mkdir(parents=True, exist_ok=True)
            content = src.read_text(encoding='utf-8')
            content = content.replace('GENERATE_AT_INSTALL', secrets.token_hex(12))
            dst.write_text(content, encoding='utf-8')
    (root / 'installed.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
    print('Release installed. Existing settings and publication mode preserved.', flush=True)


if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('--root', type=Path, default=ROOT)
    install(p.parse_args().root)
