"""External read-only GameSpy UDP probe; run outside the Helsinki node."""
import argparse
import json
import socket
import time


def query(host, port, timeout=5):
    result = {}
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.connect((host, port))
        sock.settimeout(timeout)
        sock.send(b'\\status\\')
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            packet = sock.recv(65535).decode('latin-1').rstrip('\0')
            parts = packet.strip('\\').split('\\')
            i = 0
            while i < len(parts):
                if parts[i] in ('final', ''):
                    i += 1
                    continue
                if i + 1 < len(parts):
                    result[parts[i]] = parts[i + 1]
                i += 2
            if '\\final' in packet:
                break
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('host')
    parser.add_argument('--port', type=int, default=35001)
    args = parser.parse_args()
    data = query(args.host, args.port)
    print(json.dumps(data, indent=2))
    assert data['hostport'] == '35000', 'Wrong advertised game port'
    assert data['maxplayers'] == '8', 'Wrong player limit'
    assert data['hostname'] == 'Novara | Renegade Coop', 'Wrong server name'
