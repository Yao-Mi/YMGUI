#!/usr/bin/env bash
# Rebuild and exercise the interactive ANIM scenes under both primary pixel formats.
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
scenes=(list_ops drag_snap submit loading detail pull_refresh carousel)

for depth in 16 24; do
	if [ "$depth" = 16 ]; then
		build_dir="$repo_root/build/rgb565/Demo"
	else
		build_dir="$repo_root/build/rgb888/Demo"
	fi
	cmake -S "$repo_root" -B "$build_dir" -DYMGUI_COLOR_DEPTH="$depth" -DYMGUI_ANIM=ON
	targets=(test_anim)
	for scene in "${scenes[@]}"; do
		targets+=("demo_anim_${scene}")
	done
	cmake --build "$build_dir" --target "${targets[@]}" -j8
	"$build_dir/test_anim"
	for scene in "${scenes[@]}"; do
		SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
			"$build_dir/demo_anim_${scene}" --selftest
	done
	echo "ANIM interactions: RGB${depth} OK"
done
