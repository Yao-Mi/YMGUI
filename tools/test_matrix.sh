#!/usr/bin/env bash
# Run repository checks plus the primary framebuffer test matrix.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEPTHS=(16 24)

if [ "${1:-}" = "--all-depths" ]; then
	DEPTHS=(1 8 16 24)
elif [ $# -ne 0 ]; then
	echo "usage: tools/test_matrix.sh [--all-depths]" >&2
	exit 2
fi

"$REPO_ROOT/tools/check_repo.sh"
for depth in "${DEPTHS[@]}"; do
	"$REPO_ROOT/tools/test.sh" --depth "$depth"
done

echo "Test matrix: OK (${DEPTHS[*]})"
