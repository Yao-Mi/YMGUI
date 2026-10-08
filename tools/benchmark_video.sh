#!/usr/bin/env bash
set -euo pipefail
video_bench_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
video_bench_out="$video_bench_root/build/checks/video-benchmark"
mkdir -p "$video_bench_out" "$video_bench_root/build/logs/video-benchmark"
exec > >(tee "$video_bench_root/build/logs/video-benchmark/results.log") 2>&1
printf 'Host: '; uname -sm
cc --version | head -1
video_bench_flags=()
while IFS= read -r video_bench_dir; do video_bench_flags+=("-I$video_bench_dir"); done < <(find "$video_bench_root/YMGUI" -type d)
for video_bench_depth in 16 24; do
    echo "Color depth: $video_bench_depth"
    cmake -S "$video_bench_root/sdk/library" -B "$video_bench_out/rgb$video_bench_depth" \
        -DCMAKE_BUILD_TYPE=Release -DYMGUI_COLOR_DEPTH="$video_bench_depth" -DYMGUI_SDK_BUILD_SDL=OFF \
        > "$video_bench_out/config$video_bench_depth.log" 2>&1
    cmake --build "$video_bench_out/rgb$video_bench_depth" -j4 > "$video_bench_out/build$video_bench_depth.log" 2>&1
    for video_bench_case in black_fill staged_flush image_content; do
        cc -O3 -std=gnu99 -DYMGUI_COLOR_DEPTH="$video_bench_depth" "${video_bench_flags[@]}" \
            "$video_bench_root/tools/benchmarks/$video_bench_case.c" "$video_bench_out/rgb$video_bench_depth/libymgui.a" \
            -lm -ldl -o "$video_bench_out/$video_bench_case$video_bench_depth"
        "$video_bench_out/$video_bench_case$video_bench_depth"
    done
    video_bench_source=()
    if [ -n "${BENCHMARK_PIPELINE_SOURCE:-}" ]; then
        video_bench_source+=("-DPIPELINE_SOURCE=\"$BENCHMARK_PIPELINE_SOURCE\"" "-DVIDEO_BUFFER_BASELINE=1")
    fi
    cc -O3 -std=gnu99 -DYMGUI_COLOR_DEPTH="$video_bench_depth" "${video_bench_flags[@]}" \
        -I"$video_bench_root/project_Demo/video_player" "${video_bench_source[@]}" \
        "$video_bench_root/project_Demo/video_player/tests/test_buffers.c" \
        "$video_bench_out/rgb$video_bench_depth/libymgui.a" $(pkg-config --cflags --libs sdl2) \
        -lm -ldl -o "$video_bench_out/video_buffers$video_bench_depth"
    "$video_bench_out/video_buffers$video_bench_depth"
done
