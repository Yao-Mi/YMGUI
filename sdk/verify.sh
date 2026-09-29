#!/usr/bin/env bash
# Verify the actual release archive without leaving extracted copies or CMake caches.
set -euo pipefail
sdk_repo_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
if [ "$#" -gt 1 ] || [ "${1:-}" = --help ] || [ "${1:-}" = -h ]; then
    echo 'usage: sdk/verify.sh [path/to/YMGUI_libs-linux-ARCH.tar.gz]'
    if [ "$#" -gt 1 ]; then exit 2; fi
    exit 0
fi
sdk_archive="${1:-$sdk_repo_dir/releases/YMGUI_libs-linux-$(uname -m).tar.gz}"
sdk_archive="$(realpath -- "$sdk_archive")"
test -f "$sdk_archive"
test -f "$sdk_archive.sha256"
mkdir -p "$sdk_repo_dir/build/checks" "$sdk_repo_dir/build/logs/sdk"
exec > >(tee "$sdk_repo_dir/build/logs/sdk/verify-archive.log") 2>&1
sdk_verify_dir="$(mktemp -d "$sdk_repo_dir/build/checks/sdk-verify.XXXXXX")"
# The target is exclusively the directory returned by mktemp in this invocation.
trap 'rm -rf -- "$sdk_verify_dir"' EXIT
(cd -- "$(dirname -- "$sdk_archive")" && sha256sum -c "$(basename -- "$sdk_archive").sha256")
tar -xzf "$sdk_archive" -C "$sdk_verify_dir"
sdk_package="$sdk_verify_dir/YMGUI_libs"
(cd "$sdk_package" && sha256sum -c checksums.sha256)
sdk_include_flags=()
while IFS= read -r sdk_header_dir; do
    sdk_include_flags+=("-I$sdk_header_dir")
done < <(find "$sdk_package/include/YMGUI" -type d)
for sdk_depth in 16 24; do
    "$sdk_package/build_demos.sh" "$sdk_depth"
    # Exercise the released archive and its matching headers, not the source library.
    cc -std=gnu99 -DYMGUI_COLOR_DEPTH="$sdk_depth" -DYMGUI_SDK_COLOR_DEPTH="$sdk_depth" \
        -DYMGUI_ANIM=1 -DYMGUI_SDK_ANIM=1 "${sdk_include_flags[@]}" \
        "$sdk_repo_dir/tests/test_anim.c" "$sdk_package/lib/libymgui_rgb$sdk_depth.a" \
        -lm -ldl -o "$sdk_verify_dir/test-anim$sdk_depth"
    "$sdk_verify_dir/test-anim$sdk_depth"
done
echo 'Release archive checks: OK (RGB565/RGB888 examples, ANIM, checksums)'
