"""Wall-clock GameSpy announcements; Dragonade continues to answer queries."""
import json
import re
import socket
import time

MASTERS = (
    'gamespy.tiberiantechnologies.org', 'zenmasters.cncirc.net',
    'renmaster.cncirc.net', 'renmaster-backup.cncirc.net',
    'renmaster-backup2.cncirc.net', 'renmaster-backup3.cncirc.net',
    'renmaster-backup4.cncirc.net',
)
INTERVAL = 25


def publication_mode(root):
    path = root / 'publication.json'
    if not path.exists():
        return 'development'
    mode = json.loads(path.read_text(encoding='utf-8')).get('mode')
    if mode not in ('development', 'public-test'):
        raise RuntimeError('publication.json must select development or public-test.')
    return mode


def validate_settings(mode, cfg, da):
    if da.get('General', 'GameSpyMasterServers') != '127.0.0.1':
        raise RuntimeError('Disable Dragonade legacy heartbeats: GameSpyMasterServers=127.0.0.1.')
    if not da.getboolean('General', 'EnableGameSpy') or da.getint('General', 'GameSpyQueryPort') != 35001:
        raise RuntimeError('Dragonade must answer GameSpy queries on port 35001.')
    if mode == 'development':
        if not cfg.getboolean('Settings', 'IsPassworded') or not cfg.get('Settings', 'bPassword'):
            raise RuntimeError('Development server requires its generated test password.')
    elif cfg.getboolean('Settings', 'IsPassworded'):
        raise RuntimeError('Public test mode requires IsPassworded=no.')


def heartbeat_packet():
    return b'\\heartbeat\\35001\\gamename\\ccrenegade\\final'


def query_ready():
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
            probe.settimeout(1)
            probe.connect(('127.0.0.1', 35001))
            probe.send(b'\\info\\')
            packet = probe.recv(8192)
            return b'\\hostname\\' in packet and b'\\mapname\\' in packet
    except OSError:
        return False


def broadcast(stopping, masters=MASTERS):
    """Independent of paused game time. Only announce after the query socket is ready."""
    addresses = {}
    last_result = {}
    resolved_at = -float('inf')
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.settimeout(0.5)
        while not stopping.is_set():
            if not query_ready():
                stopping.wait(2)
                continue
            now = time.monotonic()
            if now - resolved_at >= 300:
                found = {}
                for host in masters:
                    try:
                        for entry in socket.getaddrinfo(host, 27900, socket.AF_INET, socket.SOCK_DGRAM):
                            found.setdefault(entry[4], host)
                    except OSError as exc:
                        print(f'[GameSpy] DNS unavailable for {host}: {exc}', flush=True)
                addresses = found
                resolved_at = now
            for address, host in addresses.items():
                try:
                    sock.sendto(heartbeat_packet(), address)
                except OSError as exc:
                    print(f'[GameSpy] Heartbeat failed for {host}: {exc}', flush=True)
            deadline = time.monotonic() + INTERVAL
            while not stopping.is_set() and time.monotonic() < deadline:
                try:
                    data, sender = sock.recvfrom(4096)
                except OSError:
                    continue
                if sender not in addresses:
                    continue
                result = re.search(rb'\\heartbeatresult\\(\d+)(?:\\|$)', data)
                if result:
                    code = int(result.group(1))
                    if last_result.get(sender) != code:
                        status = 'listed' if code == 0 else 'rejected'
                        print(f'[GameSpy] {addresses[sender]}: {status} (heartbeatresult={code}).', flush=True)
                        last_result[sender] = code
