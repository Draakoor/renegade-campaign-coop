"""Probe an empty, private native FDS with M01-M11 rotation.

Requires CampaignDiagnostics=1. Advances maps with gameover; never use on a
live session. Synthetic engine checks do not substitute for client playtests.
"""
import argparse
import json
from pathlib import Path
import re
import time

from query import query


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--host', required=True)
    parser.add_argument('--port', type=int, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--services', action='store_true', help='Also probe portals and supply replenishment on M06/M07/M09')
    args = parser.parse_args()
    log = args.root / 'panel-console.log'

    def command(value):
        status = query(args.host, args.port)
        if status['numplayers'] != '0' or status.get('password') != '1':
            raise RuntimeError('Refusing to change a public or occupied server')
        with (args.root / 'console.in').open('a', encoding='ascii') as stream:
            stream.write(value + '\n')

    def wait(pattern, offset, seconds=30):
        until = time.monotonic() + seconds
        while time.monotonic() < until:
            data = log.read_text(errors='replace')[offset:]
            if re.search(pattern, data):
                return data
            time.sleep(.25)
        raise RuntimeError('Timed out: ' + pattern)

    results = []
    maps = [f'M{i:02d}' for i in range(1, 12)]
    for index, name in enumerate(maps):
        status = query(args.host, args.port)
        assert status['mapname'].casefold() == (name+'.mix').casefold(), status
        offset = len(log.read_text(errors='replace'))
        command('campaign_test_features')
        output = wait(r'DIAGNOSTIC features result=', offset)
        assert re.search(r'features result=PASS skins=([1-9][0-9]*)', output), output
        row = {'map': name, 'human_clients': 0,
               'features': [line for line in output.splitlines() if '[Novara]' in line]}
        if name in ('M06', 'M07', 'M09'):
            offset = len(log.read_text(errors='replace'))
            command('campaign_test_player_armor')
            output = wait(r'DIAGNOSTIC armor fixed_after_hit=', offset)
            assert 'result=PASS' in output, output
            row['armor'] = [line for line in output.splitlines() if 'DIAGNOSTIC' in line]
            if args.services:
                offset = len(log.read_text(errors='replace'))
                command('campaign_test_portals')
                output = wait(r'DIAGNOSTIC portal result=', offset)
                assert 'portal result=PASS' in output, output
                row['portal'] = [line for line in output.splitlines() if '[Novara]' in line]
                offset = len(log.read_text(errors='replace'))
                command('campaign_test_supplies')
                output = wait(r'DIAGNOSTIC resupply result=', offset)
                assert 'resupply result=PASS' in output and 'terminal_objects=1' in output, output
                row['supplies'] = [line for line in output.splitlines() if '[Novara]' in line]
        offset = len(log.read_text(errors='replace'))
        command('campaign_audit_map')
        wait(r'Map audit written:', offset)
        audit = (args.root / 'map-audit.tsv').read_bytes()
        (args.output.parent / (name + '-map-audit.tsv')).write_bytes(audit)
        results.append(row)
        args.output.write_text(json.dumps(results, indent=2) + '\n')
        print(name + ': PASS (synthetic, zero clients)', flush=True)
        if index < len(maps)-1:
            offset = len(log.read_text(errors='replace'))
            command('gameover')
            wait(re.escape('[Novara] Resupply map='+maps[index+1]+'.mix'), offset)
            time.sleep(.75)
    print('All eleven map probes passed; mission playthroughs remain untested.', flush=True)


if __name__ == '__main__':
    main()
