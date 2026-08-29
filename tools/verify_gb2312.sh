#!/usr/bin/env bash
# Verify the checked-in GB2312 blob and optionally reproduce it from the font.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BLOB="$REPO_ROOT/tools/gb2312_glyphs.bin"
INDEX="$REPO_ROOT/YMGUI/CORE/YMGUI_FontDataGB2312.c"
EXPECTED_SIZE=953344
EXPECTED_COUNT=7448
EXPECTED_SHA256="c2c42b2c1c2041c158a13b162549a03504202887d8179f476f9ef1a152d51316"
DO_REBUILD=0

if [ "${1:-}" = "--rebuild" ]; then
	DO_REBUILD=1
elif [ $# -ne 0 ]; then
	echo "usage: tools/verify_gb2312.sh [--rebuild]" >&2
	exit 2
fi

[ -f "$BLOB" ] || { echo "missing $BLOB" >&2; exit 1; }
[ -f "$INDEX" ] || { echo "missing $INDEX" >&2; exit 1; }

actual_size="$(wc -c < "$BLOB" | tr -d ' ')"
actual_sha256="$(sha256sum "$BLOB" | awk '{print $1}')"
actual_count="$(sed -n 's/.*YMGUI_GB2312_glyph_count = \([0-9][0-9]*\).*/\1/p' "$INDEX")"

[ "$actual_size" = "$EXPECTED_SIZE" ] || { echo "GB2312 size: $actual_size, expected $EXPECTED_SIZE" >&2; exit 1; }
[ "$actual_sha256" = "$EXPECTED_SHA256" ] || { echo "GB2312 sha256: $actual_sha256, expected $EXPECTED_SHA256" >&2; exit 1; }
[ "$actual_count" = "$EXPECTED_COUNT" ] || { echo "GB2312 glyph count: $actual_count, expected $EXPECTED_COUNT" >&2; exit 1; }

echo "GB2312 blob: OK ($actual_count glyphs, $actual_size bytes)"

if [ "$DO_REBUILD" -eq 0 ]; then
	exit 0
fi

python3 -c 'from PIL import Image, ImageDraw, ImageFont' >/dev/null 2>&1 || {
	echo "Pillow is required for --rebuild (install python3-pil)" >&2
	exit 1
}
[ -f /usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc ] || {
	echo "NotoSansCJK-Regular.ttc is required for --rebuild (install fonts-noto-cjk)" >&2
	exit 1
}

TMP_DIR="$(mktemp -d "${TMPDIR:-/tmp}/ymgui-gb2312.XXXXXX")"
trap 'rm -rf "$TMP_DIR"' EXIT

python3 "$REPO_ROOT/tools/gen_font.py" \
	"$TMP_DIR/YMGUI_FontDataGB2312.c" \
	--cjk --gb2312 --extern \
	--bin "$TMP_DIR/gb2312_glyphs.bin"

cmp "$TMP_DIR/gb2312_glyphs.bin" "$BLOB"
cmp "$TMP_DIR/YMGUI_FontDataGB2312.c" "$INDEX"
echo "GB2312 rebuild: reproducible"
