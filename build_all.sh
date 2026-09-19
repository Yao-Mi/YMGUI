#!/usr/bin/env bash
# ==============================================================================
# build_all.sh —— 一键重建整个 YMGUI 仓库的所有可执行产物。
#
#   根 CMake 编译库本体 + Demo/ 下的 demo/test,统一放 build/<色深>/Demo/;
#   project_Demo/ 下每个 app 是独立 CMake 工程、有独立构建目录
#   build/<色深>/project_Demo/<name>/,根 CMake 完全不碰它们
#   (改完 app 只 build 顶层会跑到旧二进制)。本脚本把两边一次性建齐。
#
#   用法:
#     ./build_all.sh            # 增量重建顶层 + 所有 project_Demo
#     ./build_all.sh --depth 24 # RGB888;跳过仅支持 RGB565 的 video_stidio
#     ./build_all.sh -c         # 仅清空当前色深的 Demo 和各应用构建目录
#     ./build_all.sh -t         # 顶层建完顺带跑 ctest
#     ./build_all.sh -c -t      # 全新重建 + 跑单测;不会删除 SDK 或其他色深
#
#   判定成败以命令 exit code 为准(工程铁律,不看 stdout 文字)。
#   任一单元失败 → 脚本最终退出码非 0,末尾汇总列出。
# ==============================================================================

set -u

# 始终以脚本所在目录(仓库根)为工作目录,无论从哪调用
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$REPO_ROOT"

JOBS="${YMGUI_BUILD_JOBS:-$(nproc 2>/dev/null || echo 4)}"
if ! [[ "$JOBS" =~ ^[1-9][0-9]*$ ]]; then
	echo "YMGUI_BUILD_JOBS must be a positive integer" >&2
	exit 2
fi
if [ -z "${YMGUI_BUILD_JOBS:-}" ] && [ "$JOBS" -gt 8 ]; then JOBS=8; fi
DO_CLEAN=0
DO_TEST=0
DEPTH=16

usage() {
	sed -n '2,/^$/p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
	exit "${1:-0}"
}

while [ $# -gt 0 ]; do
	case "$1" in
		-c|--clean) DO_CLEAN=1 ;;
		-t|--test)  DO_TEST=1 ;;
		--depth) DEPTH="${2:?missing depth}"; shift ;;
		-h|--help)  usage 0 ;;
		*) echo "未知参数: $1" >&2; usage 1 ;;
	esac
	shift
done

case "$DEPTH" in
	16) BUILD_BASE="build/rgb565" ;;
	24) BUILD_BASE="build/rgb888" ;;
	*) echo "--depth must be 16 or 24" >&2; exit 2 ;;
esac
DEMO_BUILD="$BUILD_BASE/Demo"

# 颜色(非终端时禁用)
if [ -t 1 ]; then
	C_OK=$'\033[32m'; C_ERR=$'\033[31m'; C_HD=$'\033[36m'; C_RST=$'\033[0m'
else
	C_OK=; C_ERR=; C_HD=; C_RST=
fi

PASS_LIST=()
FAIL_LIST=()

# build_unit <标签> <源码目录> <构建目录>
#   每次 configure 以确认色深,然后增量 build。任一步失败记入 FAIL_LIST。
build_unit() {
	local label="$1" src="$2" bdir="$3"
	echo "${C_HD}==> [$label]${C_RST} $src -> $bdir"

	if [ "$DO_CLEAN" -eq 1 ]; then
		if ! rm -rf "$bdir"; then
			FAIL_LIST+=("$label (clean)")
			return 1
		fi
	fi

	if ! cmake -S "$src" -B "$bdir" -DYMGUI_COLOR_DEPTH="$DEPTH"; then
		echo "${C_ERR}    configure 失败${C_RST}"
		FAIL_LIST+=("$label (configure)")
		return 1
	fi

	if ! cmake --build "$bdir" -j "$JOBS"; then
		echo "${C_ERR}    build 失败${C_RST}"
		FAIL_LIST+=("$label (build)")
		return 1
	fi

	echo "${C_OK}    OK${C_RST}"
	PASS_LIST+=("$label")
	return 0
}

# ---- 1) 顶层:库 + 全部 Demo/ 下 demo/test ----
TOP_OK=0
if build_unit "Demo (lib + demos + tests, depth $DEPTH)" "." "$DEMO_BUILD"; then
	TOP_OK=1
fi

# ---- 2) 可选:跑顶层单测 ----
if [ "$DO_TEST" -eq 1 ] && [ "$TOP_OK" -eq 1 ]; then
	echo "${C_HD}==> [ctest]${C_RST} 顶层单测"
	if ctest --test-dir "$DEMO_BUILD" --output-on-failure; then
		echo "${C_OK}    ctest 全过${C_RST}"
		PASS_LIST+=("ctest")
	else
		echo "${C_ERR}    ctest 有失败${C_RST}"
		FAIL_LIST+=("ctest")
	fi
fi

# ---- 3) 遍历 project_Demo/ 下每个独立工程 ----
for cml in project_Demo/*/CMakeLists.txt; do
	[ -e "$cml" ] || continue          # 无匹配时跳过(nullglob 兜底)
	name="$(basename "$(dirname "$cml")")"
	if [ "$name" = video_stidio ] && [ "$DEPTH" != 16 ]; then
		echo "SKIP project_Demo/video_stidio: 仅支持 RGB565"
		continue
	fi
	build_unit "project_Demo/$name (depth $DEPTH)" "project_Demo/$name" "$BUILD_BASE/project_Demo/$name"
done

# ---- 汇总 ----
echo
echo "${C_HD}================ 汇总 ================${C_RST}"
for p in "${PASS_LIST[@]:-}"; do
	[ -n "$p" ] && echo "  ${C_OK}PASS${C_RST}  $p"
done
if [ "${#FAIL_LIST[@]}" -gt 0 ]; then
	for f in "${FAIL_LIST[@]}"; do
		echo "  ${C_ERR}FAIL${C_RST}  $f"
	done
	echo "${C_ERR}有 ${#FAIL_LIST[@]} 个单元失败${C_RST}"
	exit 1
fi

echo "${C_OK}全部构建成功${C_RST}"
exit 0
