#!/usr/bin/env python3
"""Build, sign, install and launch on an already paired Apple Watch over its paired developer connection."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--device', required=True, help='Paired device name, CoreDevice identifier, or UDID')
    parser.add_argument('--team', default=os.environ.get('ICE_DEVELOPMENT_TEAM'), help='Apple development team ID')
    parser.add_argument('--bundle-id', default=os.environ.get('ICE_BUNDLE_ID', 'com.binofet.ice.companion.watch'))
    args = parser.parse_args()
    if not args.team:
        parser.error('Set ICE_DEVELOPMENT_TEAM or pass --team after signing into Xcode. No Apple password is needed here.')
    root = Path(__file__).resolve().parents[3]
    reports = root / 'build' / 'platform-reports'
    reports.mkdir(parents=True, exist_ok=True)
    listing = reports / 'apple-devices.json'
    subprocess.run(['xcrun', 'devicectl', 'list', 'devices', '--json-output', str(listing)], check=True, stdout=subprocess.DEVNULL)
    devices = json.loads(listing.read_text())['result']['devices']
    matches = [d for d in devices if args.device in (d.get('identifier'), d.get('hardwareProperties', {}).get('udid'), d.get('deviceProperties', {}).get('name'))]
    if len(matches) != 1:
        parser.error('Device selector must match exactly one paired device; check Xcode Devices and Simulators.')
    device = matches[0]
    hardware, properties = device['hardwareProperties'], device['deviceProperties']
    if hardware.get('platform') != 'watchOS' or hardware.get('reality') != 'physical':
        parser.error('Select a physical Apple Watch.')
    if properties.get('developerModeStatus') != 'enabled':
        parser.error('Enable Settings > Privacy & Security > Developer Mode, restart, and confirm on the device.')
    environment = dict(os.environ, ICE_DEVELOPMENT_TEAM=args.team, ICE_BUNDLE_ID=args.bundle_id, ICE_WATCH_DESTINATION='id=' + hardware['udid'])
    subprocess.run([str(root / 'examples/mobile/watchos/build.sh'), 'device'], check=True, env=environment)
    app = root / 'build/mobile/watchos-device/Debug-watchos/ICEWatchProbe.app'
    subprocess.run(['xcrun', 'devicectl', 'device', 'install', 'app', '--device', device['identifier'], str(app)], check=True)
    subprocess.run(['xcrun', 'devicectl', 'device', 'process', 'launch', '--device', device['identifier'], '--terminate-existing', args.bundle_id], check=True)
    print('Companion installed and launched on ' + properties['name'])


if __name__ == '__main__':
    try:
        main()
    except subprocess.CalledProcessError as error:
        sys.exit(error.returncode)
