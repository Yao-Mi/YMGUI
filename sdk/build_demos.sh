#!/usr/bin/env bash
set -euo pipefail
ymgui_sdk_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ymgui_depth="${1:-16}"
case "$ymgui_depth" in 16|24) ;; *) echo 'usage: ./build_demos.sh [16|24]' >&2; exit 2 ;; esac
cmake -S "$ymgui_sdk_dir/examples" -B "$ymgui_sdk_dir/examples-build$ymgui_depth" \
    -DYMGUI_COLOR_DEPTH="$ymgui_depth" -DCMAKE_BUILD_TYPE=Release
cmake --build "$ymgui_sdk_dir/examples-build$ymgui_depth" -j4
ctest --test-dir "$ymgui_sdk_dir/examples-build$ymgui_depth" --output-on-failure
