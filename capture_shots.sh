#!/usr/bin/env bash
# ==============================================================================
# capture_shots.sh —— 批量给所有 demo/app 截图,产物落 docs/shots/<name>.png。
#
#   原理:SDL_LCD 认环境变量 YMGUI_SHOT=<path.bmp>,退出时把最后一帧全屏 buffer
#   存成 BMP(见 SDL_LCD.c)。本脚本对每个可执行:
#     SDL_VIDEODRIVER=dummy(无需真显示器)+ YMGUI_SHOT=<bmp> + timeout 兜底
#     跑固定帧数 → 得 BMP → ffmpeg 转 PNG → 删 BMP。
#   交互类(dropdown/msgbox/alarm_clock)在 demo 内部靠 YMGUI_SHOT 自动触发一次
#   交互(展开浮层/弹模态),故截到的是有代表性的画面。
#
#   用法:
#     ./capture_shots.sh          # 用现有二进制截图(缺了会提示先 build)
#     ./capture_shots.sh -b        # 先 ./build_all.sh 再截
#
#   判定成败以 exit code 为准;任一截图失败 → 末尾汇总 + 退出码非 0。
# ==============================================================================

set -u

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$REPO_ROOT"

OUT="docs/shots"
FRAMES=40          # 跑够帧数让动画/缓动稳定
TIMEOUT=20         # 单个 demo 最长跑多少秒(兜底防挂)
DO_BUILD=0

while [ $# -gt 0 ]; do
	case "$1" in
		-b|--build) DO_BUILD=1 ;;
		-h|--help)  sed -n '2,22p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 0 ;;
		*) echo "未知参数: $1" >&2; exit 1 ;;
	esac
	shift
done

if [ -t 1 ]; then
	C_OK=$'\033[32m'; C_ERR=$'\033[31m'; C_HD=$'\033[36m'; C_RST=$'\033[0m'
else
	C_OK=; C_ERR=; C_HD=; C_RST=
fi

command -v ffmpeg >/dev/null || { echo "${C_ERR}需要 ffmpeg 把 BMP 转 PNG${C_RST}"; exit 1; }

if [ "$DO_BUILD" -eq 1 ]; then
	echo "${C_HD}==> 先构建全部${C_RST}"
	./build_all.sh || { echo "${C_ERR}构建失败,终止截图${C_RST}"; exit 1; }
fi

mkdir -p "$OUT"
OK_LIST=()
FAIL_LIST=()

# shoot <name> <exe 路径>
#   跑 exe(dummy 驱动 + YMGUI_SHOT)→ BMP → PNG。exe 不存在/超时/无 BMP 记失败。
shoot() {
	local name="$1" exe="$2"
	local bmp="$OUT/$name.bmp" png="$OUT/$name.png"
	if [ ! -x "$exe" ]; then
		echo "${C_ERR}  跳过 $name:可执行不存在 ($exe) —— 先 ./build_all.sh${C_RST}"
		FAIL_LIST+=("$name (缺二进制)")
		return 1
	fi
	rm -f "$bmp"
	SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy YMGUI_SHOT="$bmp" \
		timeout "$TIMEOUT" "$exe" "$FRAMES" >/dev/null 2>&1
	if [ ! -s "$bmp" ]; then
		echo "${C_ERR}  失败 $name:没生成 BMP${C_RST}"
		FAIL_LIST+=("$name (无输出)")
		return 1
	fi
	if ffmpeg -y -i "$bmp" "$png" >/dev/null 2>&1; then
		rm -f "$bmp"
		local dims; dims=$(ffprobe -v error -show_entries stream=width,height -of csv=p=0 "$png" 2>/dev/null)
		echo "${C_OK}  OK   $name  ($dims)${C_RST}"
		OK_LIST+=("$name")
	else
		echo "${C_ERR}  失败 $name:ffmpeg 转换出错${C_RST}"
		FAIL_LIST+=("$name (转换失败)")
		return 1
	fi
}

# ---- 1) Demo/ 下 24 个单控件 demo(顶层 build/ 产物)----
echo "${C_HD}==> Demo/ 单控件演示${C_RST}"
for exe in build/demo_*; do
	[ -x "$exe" ] || continue
	[ -f "$exe" ] || continue          # 排除目录
	shoot "$(basename "$exe")" "$exe"
done

# ---- 2) project_Demo/ 下 9 个完整应用(各自 build 目录)----
echo "${C_HD}==> project_Demo/ 完整应用${C_RST}"
for d in project_Demo/*/; do
	name="$(basename "$d")"
	exe="build/project_Demo/$name/$name"
	[ -f "project_Demo/$name/CMakeLists.txt" ] || continue
	shoot "$name" "$exe"
done

# ---- 汇总 ----
echo
echo "${C_HD}============ 截图汇总 ============${C_RST}"
echo "  成功 ${#OK_LIST[@]} 张 → $OUT/"
if [ "${#FAIL_LIST[@]}" -gt 0 ]; then
	for f in "${FAIL_LIST[@]}"; do echo "  ${C_ERR}FAIL${C_RST}  $f"; done
	echo "${C_ERR}有 ${#FAIL_LIST[@]} 个失败${C_RST}"
	exit 1
fi
echo "${C_OK}全部截图完成${C_RST}"
exit 0
