"""Generate the importable Pterodactyl PTDL_v2 egg from the pinned image."""
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
lock = json.loads((root / 'sources.lock.json').read_text())
image = 'ghcr.io/draakoor/renegade-campaign-coop:main'
egg = {
    '_comment': 'Private development build. Full campaign acceptance is pending.',
    'meta': {'version': 'PTDL_v2', 'update_url': None},
    'exported_at': '2026-10-19T00:00:00+00:00',
    'name': 'Renegade Campaign Coop',
    'author': 'admin@wellisch-itservice.de',
    'description': 'TT 4.8.4 / DA 1.11.1 with M06/M07/M09 coop adaptations. Supply CoopBeta424.zip, server.dat and the original M06/M07/M09 .mix and .LVL files in input/. Set TT_DOWNLOAD_URL. First start assembles a private full-campaign test rotation; human playtests pending.',
    'features': [],
    'docker_images': {'Renegade Campaign Coop (development)': image},
    'file_denylist': [],
    'startup': 'python3 -u /opt/renegade-coop/runtime.py',
    'config': {
        'files': '{}',
        'startup': json.dumps({'done': 'Loaded OK'}),
        'logs': '{}', 'stop': 'quit'
    },
    'scripts': {'installation': {
        'script': '#!/bin/sh\nset -eu\nmkdir -p /mnt/server/input\nprintf "%s\\n" "Supply CoopBeta424.zip, server.dat and M06/M07/M09 .mix + .LVL in input/, configure TT_DOWNLOAD_URL, then start. Existing data is preserved."\n',
        'container': image, 'entrypoint': '/bin/sh'
    }},
    'variables': [
        {'name': 'TT download URL', 'description': 'Public HTTP(S) address of this server download endpoint, including TCP port 35002.',
         'env_variable': 'TT_DOWNLOAD_URL', 'default_value': 'http://YOUR_PUBLIC_IP:35002/', 'user_viewable': True,
         'user_editable': True, 'rules': 'required|string|url', 'field_type': 'text'},
        {'name': 'Release mode', 'description': 'Default is private. Use tools/set_public.py on a stopped server for explicit public testing.',
         'env_variable': 'RELEASE_MODE', 'default_value': 'development', 'user_viewable': True,
         'user_editable': False, 'rules': 'required|in:development', 'field_type': 'text'},
        {'name': 'Disable automatic image entrypoint updates', 'description': 'Runtime versions are pinned.',
         'env_variable': 'AUTO_UPDATE', 'default_value': '0', 'user_viewable': False,
         'user_editable': False, 'rules': 'required|in:0', 'field_type': 'text'}
    ]
}
(root / 'egg-renegade-campaign-coop.json').write_text(json.dumps(egg, indent=2), encoding='utf-8')
