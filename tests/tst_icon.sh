#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd "$(dirname "$0")" && pwd)
svg="${1:-$script_dir/../assets/zapshell.svg}"

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

python3 - "$script_dir/../assets/zapshell-tray.svg" "$script_dir/../src/tray.cpp" "$script_dir/../PKGBUILD" <<'PY'
import sys
import xml.etree.ElementTree as ET

tray_svg, tray_cpp, pkgbuild = sys.argv[1:4]
root = ET.parse(tray_svg).getroot()
assert root.find('{*}rect') is None, 'ícone da bandeja não deve ter fundo'
assert root.get('viewBox') == '6 3 52 52', root.get('viewBox')
colors = {(e.get(a) or '').lower() for e in root.iter() for a in ('fill', 'stroke')}
assert {'#25d366', '#ffffff', '#ffd54f'} <= colors, colors
assert 'fromTheme(QStringLiteral("zapshell-tray"))' in open(tray_cpp, encoding='utf-8').read()
assert 'zapshell-tray.svg' in open(pkgbuild, encoding='utf-8').read()
PY

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
for size in 32 48 128; do
    rsvg-convert -w "$size" -h "$size" "$svg" -o "$tmp/$size.png"
    test -s "$tmp/$size.png"
done

python3 - "$tmp" <<'PY'
import os
import sys
from PIL import Image

tmp = sys.argv[1]
palette = {(0x16, 0x23, 0x2b), (0x25, 0xd3, 0x66), (0xff, 0xff, 0xff), (0xff, 0xd5, 0x4f)}


def pixels(image):
    data = image.get_flattened_data() if hasattr(image, 'get_flattened_data') else image.getdata()
    return list(data)


for size in (32, 48, 128):
    art = Image.open(os.path.join(tmp, f'{size}.png')).convert('RGBA')
    visible = {p[:3] for p in pixels(art) if p[3] > 200}
    assert palette <= visible, f'{size}px: faltam {palette - visible}'

    for background in ((255, 255, 255, 255), (11, 14, 17, 255)):
        canvas = Image.new('RGBA', art.size, background)
        canvas.alpha_composite(art)
        seen = {p[:3] for p in pixels(canvas)}
        assert {(0x25, 0xd3, 0x66), (0xff, 0xd5, 0x4f)} <= seen, f'{size}px fundo {background}: arte sumiu'
PY
