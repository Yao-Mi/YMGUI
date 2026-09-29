#!/usr/bin/env bash
# Verify that disabling ANIM removes its symbols and targets without breaking ordinary UI.
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${1:-$repo_root/build/checks/anim-off}"

cmake -S "$repo_root" -B "$build_dir" -DYMGUI_COLOR_DEPTH=16 -DYMGUI_ANIM=OFF
cmake --build "$build_dir" --target ymgui demo_button test_event -j8

if nm -g --defined-only "$build_dir/libymgui.a" | rg 'YMGUI_Anim_'; then
	echo "ANIM symbols remain in the trimmed library" >&2
	exit 1
fi
if cmake --build "$build_dir" --target help | rg '^\.\.\. (demo_anim|test_anim)'; then
	echo "ANIM demo/test targets remain in the trimmed build" >&2
	exit 1
fi

ctest --test-dir "$build_dir" -R '^test_event$' --output-on-failure
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy "$build_dir/demo_button" 10
echo "ANIM trim: OK"
