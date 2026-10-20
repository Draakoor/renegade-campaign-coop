"""Pterodactyl process supervisor. No host services or remote admin sockets."""
import configparser
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import sys
import threading
import time
from urllib.parse import unquote, urlsplit
from volume_lock import volume_lock
from gamespy import broadcast, publication_mode, validate_settings

ROOT = Path(os.environ.get('RENEGADE_ROOT', Path(__file__).resolve().parent)).resolve()
MAPS = [f'M{i:02d}' for i in range(1, 12)]


class PackageHandler(SimpleHTTPRequestHandler):
    """Only regular files inside the TTFS files/packages folders are served."""
    def send_head(self):
        path = unquote(urlsplit(self.path).path)
        parts = Path(path.lstrip('/')).parts
        allowed = parts and parts[0] in ('files', 'packages') and '..' not in parts
        file = Path(self.directory).joinpath(*parts).resolve()
        if not allowed or not file.is_relative_to(Path(self.directory).resolve()) or not file.is_file():
            self.send_error(404)
            return
        return super().send_head()

    def list_directory(self, path):
        self.send_error(404)
        return None

    def log_message(self, format, *args):
        if len(args) > 1 and str(args[1]).startswith(('4', '5')):
            print('[TTFS] Request failed: ' + format % args, flush=True)


def check_validation(root):
    """Fail closed: never turn incomplete campaign work into a public server."""
    evidence = json.loads((root / 'validation.json').read_text())
    required = ('completion', 'respawn', 'late_join', 'simultaneous_triggers',
                'mission_failure', 'player_leave', 'transition')
    for name in MAPS:
        row = evidence.get('missions', {}).get(name, {})
        if row.get('clients', 0) < 2 or any(row.get(k) != 'passed' for k in required):
            raise RuntimeError(f'Public release blocked: {name} has not passed two-client validation.')
        if not row.get('evidence'):
            raise RuntimeError(f'Public release blocked: no recorded evidence for {name}.')
    load = evidence.get('load_test', {})
    if load.get('clients', 0) < 8 or load.get('result') != 'passed' or not load.get('evidence'):
        raise RuntimeError('Public release blocked: eight-client load test is missing.')
    if evidence.get('network', {}).get('join_from_community_list') != 'passed':
        raise RuntimeError('Public release blocked: community-list join test is missing.')


def append_command(server, command):
    if '\n' in command or '\r' in command or len(command.encode()) > 1000:
        raise ValueError('Invalid console command')
    with (server / 'console.in').open('a', encoding='utf-8', newline='\n') as out:
        out.write(command + '\n')


def package_directory(server):
    """Respect the installed path configuration when upgrading older volumes."""
    paths = configparser.ConfigParser(interpolation=None)
    paths.read(server / 'data' / 'paths.ini')
    base = paths.get('paths', 'FileBase', fallback='NovaraRenegade')
    fds = paths.get('paths', 'FileFDS', fallback='FDS')
    if any(not re.fullmatch(r'[A-Za-z0-9_-]+', part) for part in (base, fds)):
        raise RuntimeError('paths.ini contains an invalid package directory.')
    return server / base / fds / 'ttfs'


def resume_active_mission(server):
    """FDS advances resumerotation.txt during quit; restore the last loaded map."""
    settings = configparser.ConfigParser(interpolation=None)
    settings.read(server / 'server.ini')
    if not settings.getboolean('Server', 'ResumeRotation', fallback=False):
        return
    active = server / 'active-map.txt'
    if not active.exists():
        return
    match = re.search(r'^\s*rotation\s*:\s*(\[[^\]]*\])\s*;',
                      (server / 'tt.cfg').read_text(encoding='utf-8'), re.MULTILINE)
    if not match:
        raise RuntimeError('Cannot resume mission: tt.cfg needs an explicit rotation array.')
    rotation = json.loads(match.group(1))
    if not rotation or any(not isinstance(name, str) for name in rotation):
        raise RuntimeError('Cannot resume mission: invalid rotation.')
    previous = active.read_text(encoding='utf-8').strip().casefold().removesuffix('.mix')
    names = [name.casefold().removesuffix('.mix') for name in rotation]
    index = names.index(previous) if previous in names else 0
    target = server / 'resumerotation.txt'
    temporary = server / 'resumerotation.tmp'
    temporary.write_text(str(index) + '\n', encoding='ascii')
    os.replace(temporary, target)
    print(f'[Supervisor] Resuming {rotation[index]} from its beginning (rotation position {index}).', flush=True)


def main():
    with volume_lock(ROOT):
        return run_locked()


def run_locked():
    if (ROOT / 'runtime.lock').exists():
        raise RuntimeError('Legacy runtime.lock exists; stop the old server and verify the stale lock before migration.')
    if not (ROOT / 'installed.json').exists():
        if not (ROOT / 'release' / 'manifest.json').exists():
            from tools.stage_volume import stage_volume
            stage_volume(ROOT)
        from bootstrap import install
        install(ROOT, lock_held=True)
    server = ROOT / 'server'
    mode = publication_mode(ROOT)
    if os.environ.get('RELEASE_MODE', 'development') != 'development':
        check_validation(ROOT)
        raise RuntimeError('Release validation passed; apply the reviewed public configuration before publishing.')
    cfg = configparser.ConfigParser(interpolation=None)
    cfg.read(server / 'data' / 'svrcfg_cnc.ini')
    da = configparser.ConfigParser(interpolation=None)
    da.read(server / 'da.ini')
    validate_settings(mode, cfg, da)
    resume_active_mission(server)
    stopping = threading.Event()
    stopped_at = [None]
    proc = None
    http = None
    bot = None
    bot_restarts = 0
    bot_retry_at = 0
    bridge_ready = False

    def bot_output(process):
        for line in process.stdout:
            print('[BRenBot] ' + line.rstrip(), flush=True)

    def stop(*_):
        if not stopping.is_set():
            stopping.set()
            stopped_at[0] = time.monotonic()
            print('[Supervisor] Sending quit to Renegade.', flush=True)
            append_command(server, 'quit')

    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)
    env = os.environ.copy()
    env['WINEPREFIX'] = str(ROOT / '.wine')
    env['WINEDEBUG'] = '-all'
    try:
        # vcrun2012 is required by TT; vcrun2022 by the newly built plugin.
        marker = ROOT / '.wine' / '.novara-runtime-v1'
        if not marker.exists():
            print('[Supervisor] Preparing persistent Wine prefix and Microsoft/DirectX runtimes.', flush=True)
            subprocess.run(['wineboot', '-u'], env=env, check=True)
            subprocess.run(['winetricks', '-q', 'vcrun2012', 'vcrun2022', 'd3dx9_43', 'd3dcompiler_43'], env=env, check=True)
            marker.write_text('TT4.8.4/DA1.11.1\n')
        from functools import partial
        package_root = package_directory(server)
        http = ThreadingHTTPServer(('0.0.0.0', 35002), partial(PackageHandler, directory=str(package_root)))
        threading.Thread(target=http.serve_forever, daemon=True).start()
        print('[Supervisor] TT package downloads available on TCP 35002.', flush=True)
        log = server / 'panel-console.log'
        offset = log.stat().st_size if log.exists() else 0
        with (ROOT / 'wine.log').open('ab') as wine_log:
            proc = subprocess.Popen(['wine', 'server.exe'], cwd=server, env=env,
                                    stdin=subprocess.DEVNULL, stdout=wine_log, stderr=subprocess.STDOUT)
            print(f'[Supervisor] Renegade starting in {mode} mode.', flush=True)
            if mode == 'public-test':
                print('[GameSpy] Public test enabled; campaign acceptance remains pending. Heartbeat interval: 25 seconds.', flush=True)
                threading.Thread(target=broadcast, args=(stopping,), daemon=True).start()

            def read_console():
                for line in sys.stdin:
                    line = line.rstrip('\r\n')
                    if line.lower() in ('quit', 'exit', 'stop'):
                        stop()
                        return
                    if line:
                        append_command(server, line)

            threading.Thread(target=read_console, daemon=True).start()
            while proc.poll() is None:
                if log.exists():
                    with log.open('rb') as incoming:
                        incoming.seek(offset)
                        data = incoming.read()
                        offset = incoming.tell()
                    if data:
                        print(data.decode('cp1252', errors='replace'), end='', flush=True)
                        bridge_ready |= b'[Novara] da_campaign loaded' in data
                if bridge_ready and not stopping.is_set():
                    if bot is not None and bot.poll() is not None:
                        print(f'[BRenBot] Exited: {bot.returncode}; retry in 10 seconds.', flush=True)
                        bot.stdout.close()
                        bot = None
                        bot_restarts += 1
                        bot_retry_at = time.monotonic() + 10
                    if bot is None and bot_restarts < 3 and time.monotonic() >= bot_retry_at:
                        from brenbot_runtime import start as start_bot
                        bot = start_bot(ROOT)
                        threading.Thread(target=bot_output, args=(bot,), daemon=True).start()
                    if bot_restarts == 3:
                        print('[BRenBot] Repeated startup failure; game stays online. Check bot configuration.', flush=True)
                        bot_restarts += 1
                if stopped_at[0] and time.monotonic() - stopped_at[0] > 25:
                    print('[Supervisor] Graceful stop timed out; ending this Wine prefix.', flush=True)
                    subprocess.run(['wineserver', '-k'], env=env, check=False)
                    proc.wait(timeout=10)
                time.sleep(0.2)
            print(f'[Supervisor] Renegade exited: {proc.returncode}', flush=True)
            return 0 if stopping.is_set() else proc.returncode or 1
    finally:
        stopping.set()
        from brenbot_runtime import stop as stop_bot
        stop_bot(bot)
        if http:
            http.shutdown()
            http.server_close()


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, RuntimeError, subprocess.SubprocessError, ValueError) as exc:
        print(f'[Supervisor] {exc}', file=sys.stderr)
        sys.exit(1)
