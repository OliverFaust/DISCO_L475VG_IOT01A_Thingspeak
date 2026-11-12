#!/usr/bin/env python3
"""mask_log.py -- remove identifying data from a console capture before it goes into docs/results/.

    tools/mask_log.py <raw capture> <output file>

Masks, line by line:
  - MAC addresses and BSSIDs (xx:xx:xx:xx:xx:xx): the last three bytes become XX:XX:XX; the first
    three (the manufacturer's prefix, OUI) stay. A BSSID together with an SSID can be located with
    public Wi-Fi databases; a MAC address identifies a device.
  - SSIDs in access point scan lines: the first three characters stay, the rest becomes '*', the
    length is kept ("ExampleNet2G" -> "Exa*********").
  - ThingSpeak API keys after "api_key=": the first four characters stay ("ABCD************").

The raw capture (with the real values) stays on your machine; only the masked file is committed.
Check the result with: grep -nE '([0-9A-F]{2}:){5}[0-9A-F]{2}' <output file>  (must find nothing).
"""
import re
import sys

MAC = re.compile(r'\b([0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}):[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}\b')
# Scan line printed by the bring-up test: "  <n>  <SSID padded to 32>  <BSSID>  ch ..."
SCAN = re.compile(r'^(\s+\d+\s{2})(.{32})(\s{2}[0-9A-Fa-f]{2}:)')
API_KEY = re.compile(r'(api_key=)([A-Za-z0-9]{4})([A-Za-z0-9]*)')


def mask_ssid(field: str) -> str:
    ssid = field.rstrip(' ')
    masked = ssid[:3] + '*' * max(0, len(ssid) - 3)
    return masked.ljust(len(field))


def mask_line(line: str) -> str:
    line = SCAN.sub(lambda m: m.group(1) + mask_ssid(m.group(2)) + m.group(3), line)
    line = MAC.sub(lambda m: m.group(1) + ':XX:XX:XX', line)
    line = API_KEY.sub(lambda m: m.group(1) + m.group(2) + '*' * len(m.group(3)), line)
    return line


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__, file=sys.stderr)
        return 2
    with open(sys.argv[1], encoding='utf-8', errors='replace') as f:
        lines = f.readlines()
    with open(sys.argv[2], 'w', encoding='utf-8') as f:
        f.writelines(mask_line(l) for l in lines)
    return 0


if __name__ == '__main__':
    sys.exit(main())
