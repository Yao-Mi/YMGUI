#!/usr/bin/env bash
# ==============================================================================
# build_all.sh —— 一键重建整个 YMGUI 仓库的所有可执行产物。
#
#   为什么需要它:顶层 build/ 只编译库本体 + Demo/ 下的 demo/test;
#   project_Demo/ 下每个 app 是独立 CMake 工程、有独立构建目录
#   build/rgb565/project_Demo/<name>/,顶层 `cmake --build build` 完全不碰它们
#   (改完 app 只 build 顶层会跑到旧二进制)。本脚本把两边一次性建齐。
#
#   用法:
#     ./build_all.sh            # 增量重建顶层 + 所有 project_Demo
#     ./build_all.sh -c         # 先清空所有 build 目录再全新重建
#     ./build_all.sh -t         # 顶层建完顺带跑 ctest
#     ./build_all.sh -c -t      # 全新重建 + 跑单测
#
#   判定成败以命令 exit code 为准(工程铁律,不看 stdout 文字)。
#   任一单元失败 → 脚本最终退出码非 0,末尾汇总列出。
# ==============================================================================

set -u

# 始终以脚本所在目录(仓库根)为工作目录,无论从哪调用
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$REPO_ROOT"

JOBS="$(nproc 2>/dev/null || echo 4)"
DO_CLEAN=0
DO_TEST=0

usage() {
	sed -n '2,20p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
	exit "${1:-0}"
}

while [ $# -gt 0 ]; do
	case "$1" in
		-c|--clean) DO_CLEAN=1 ;;
		-t|--test)  DO_TEST=1 ;;
		-h|--help)  usage 0 ;;
		*) echo "未知参数: $1" >&2; usage 1 ;;
	esac
	shift
done

# 颜色(非终端时禁用)
if [ -t 1 ]; then
	C_OK=$'\033[32m'; C_ERR=$'\033[31m'; C_HD=$'\033[36m'; C_RST=$'\033[0m'
else
	C_OK=; C_ERR=; C_HD=; C_RST=
fi

PASS_LIST=()
FAIL_LIST=()

# build_unit <标签> <源码目录> <构建目录>
#   configure(缺 CMakeCache 时)+ build。任一步失败记入 FAIL_LIST。
build_unit() {
	local label="$1" src="$2" bdir="$3"
	echo "${C_HD}==> [$label]${C_RST} $src -> $bdir"

	if [ "$DO_CLEAN" -eq 1 ]; then
		rm -rf "$bdir"
	fi

	if [ ! -f "$bdir/CMakeCache.txt" ]; then
		if ! cmake -S "$src" -B "$bdir"; then
			echo "${C_ERR}    configure 失败${C_RST}"
			FAIL_LIST+=("$label (configure)")
			return 1
		fi
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
build_unit "top-level (lib + demos + tests)" "." "build"

# ---- 2) 可选:跑顶层单测 ----
if [ "$DO_TEST" -eq 1 ] && [ -f "build/CMakeCache.txt" ]; then
	echo "${C_HD}==> [ctest]${C_RST} 顶层单测"
	if ( cd build && ctest --output-on-failure ); then
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
	build_unit "project_Demo/$name (rgb565)" "project_Demo/$name" "build/rgb565/project_Demo/$name"
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
