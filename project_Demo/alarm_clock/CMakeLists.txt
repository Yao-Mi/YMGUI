cmake_minimum_required(VERSION 3.16)
project(alarm_clock C)

# 咬合层:把仓库里的 YMGUI/ 当外部依赖引入(建 ymgui + sdl_lcd + SDL2)
include(${CMAKE_CURRENT_LIST_DIR}/../ymgui_app.cmake)

# 闹钟(第八个基础验证项目)。
# 本项目不催生新控件——目标是把刚落地的 MsgBox 模态弹窗放进真应用里压一遍:
# 到点 + 使能开 → 弹出阻塞模态,遮罩锁住整个界面,必须点"关闭"按钮才解锁。
# 三个 Roller(时/分/秒)设定时刻 + Switch 使能 + 帧驱动假时钟。
# 分层铁律:控件不认识"闹钟/时刻";时间推进、到点判定、去抖全在本 app 侧。
ymgui_add_app(alarm_clock
	alarm_clock.c)

# GB2312 全字库外部 blob 路径(库在 Demo/ 下生成,项目复用同一份)
target_compile_definitions(alarm_clock PRIVATE
	GB2312_BIN_PATH="${YMGUI_REPO_ROOT}/tools/gb2312_glyphs.bin")
