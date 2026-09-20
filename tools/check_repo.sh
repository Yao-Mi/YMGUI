#!/usr/bin/env bash
# Fast repository checks that do not compile the project.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

expect_count() {
	local label="$1" actual="$2" expected="$3"
	if [ "$actual" != "$expected" ]; then
		echo "$label: $actual, expected $expected" >&2
		exit 1
	fi
	echo "$label: OK ($actual)"
}

python3 tools/check_docs.py
tools/verify_gb2312.sh
git diff --check

expect_count "CTest entries" "$(rg -c '^add_test\(' CMakeLists.txt)" 35
expect_count "Demo sources" "$(find Demo -maxdepth 1 -name 'demo_*.c' | wc -l | tr -d ' ')" 28
expect_count "Project demos" "$(find project_Demo -mindepth 2 -maxdepth 2 -name CMakeLists.txt | wc -l | tr -d ' ')" 13
expect_count "Widget headers" "$(find YMGUI/WIDGET -maxdepth 1 -name 'YMGUI_*.h' | wc -l | tr -d ' ')" 27
expect_count "IME character pronunciations" "$(rg -c '^    \{' project_Demo/chinese_ime/pinyin_gb2312.inc)" 7291
expect_count "IME phrases" "$(rg -c '^    \{' project_Demo/chinese_ime/phrases_rime.inc)" 47280
python3 tools/verify_ime_dict.py

for ime_entry in \
	'{"wo'\''men", "wm", "我们", 322329u}' \
	'{"yin'\''yue", "yy", "音乐", 16358u}' \
	'{"chong'\''qing", "cq", "重庆", 8546u}' \
	'{"kan'\''kan", "kk", "看看", 59859u}' \
	'{"ni'\''de", "nd", "你的", 39676u}' \
	'{"ga'\''da", "gd", "嘎达", 50000u}' \
	'{"ni'\''ma", "nm", "尼玛", 100000u}' \
	'{"ceng", "曾", 8019u}' \
	'{"zeng", "曾", 5422u}'; do
	if ! rg -Fq "$ime_entry" project_Demo/chinese_ime/*.inc; then
		echo "missing IME dictionary entry: $ime_entry" >&2
		exit 1
	fi
done

if rg -n 'Demo/(gen_font\.py|gb2312_glyphs\.bin)' \
	README.md CMakeLists.txt YMGUI project_Demo Demo tools \
	docs/API.md docs/ARCHITECTURE.md docs/README.md docs/当前状态.md; then
	echo "stale Demo font-tool path found" >&2
	exit 1
fi

echo "Repository checks: OK"
