#!/usr/bin/env bash
set -euo pipefail
ymgui_sdk_source_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
exec python3 "$ymgui_sdk_source_dir/package.py" "$@"
