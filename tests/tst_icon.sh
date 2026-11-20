#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd "$(dirname "$0")" && pwd)
svg="$script_dir/../assets/zapshell.svg"

python3 - "$svg" <<'PY'
import sys
import xml.etree.ElementTree as ET

root = ET.parse(sys.argv[1]).getroot()
assert root.tag.endswith('svg'), root.tag
assert root.get('viewBox') == '0 0 64 64', root.get('viewBox')
assert root.find('{*}rect').get('rx') == '16'

colors = {
    (element.get(attr) or '').lower()
    for element in root.iter()
    for attr in ('fill', 'stroke')
}
assert {'#16232b', '#25d366', '#ffffff', '#ffd54f'} <= colors, colors
assert root.findall('.//{*}text') == []
assert len(root.findall('.//{*}path')) >= 4
PY

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
for size in 32 48 128; do
    rsvg-convert -w "$size" -h "$size" "$svg" -o "$tmp/$size.png"
    test -s "$tmp/$size.png"
done
