#!/usr/bin/env bash
# Configure, build and test one framebuffer color depth.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEPTH=16
BUILD_DIR=""

usage() {
	echo "usage: tools/test.sh [--depth 1|8|16|24] [--build-dir PATH]"
}

while [ $# -gt 0 ]; do
	case "$1" in
		--depth) DEPTH="${2:?missing depth}"; shift ;;
		--build-dir) BUILD_DIR="${2:?missing build directory}"; shift ;;
		-h|--help) usage; exit 0 ;;
		*) echo "unknown argument: $1" >&2; usage >&2; exit 2 ;;
	esac
	shift
done

case "$DEPTH" in
	1|8|16|24) ;;
	*) echo "invalid color depth: $DEPTH" >&2; exit 2 ;;
esac

if [ -z "$BUILD_DIR" ]; then
	BUILD_DIR="$REPO_ROOT/build/verify/rgb$DEPTH"
elif [[ "$BUILD_DIR" != /* ]]; then
	BUILD_DIR="$REPO_ROOT/$BUILD_DIR"
fi

JOBS="${YMGUI_TEST_JOBS:-$(nproc 2>/dev/null || echo 4)}"
if ! [[ "$JOBS" =~ ^[1-9][0-9]*$ ]]; then
	echo "YMGUI_TEST_JOBS must be a positive integer" >&2
	exit 2
fi
if [ -z "${YMGUI_TEST_JOBS:-}" ] && [ "$JOBS" -gt 8 ]; then
	JOBS=8
fi
cmake -S "$REPO_ROOT" -B "$BUILD_DIR" -DYMGUI_COLOR_DEPTH="$DEPTH"
cmake --build "$BUILD_DIR" -j "$JOBS"
ctest --test-dir "$BUILD_DIR" --output-on-failure

echo "RGB$DEPTH build and tests: OK"
