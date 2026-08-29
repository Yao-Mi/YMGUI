cmake_minimum_required(VERSION 3.16)
project(dashboard C)

# 咬合层:把仓库里的 YMGUI/ 当外部依赖引入(建 ymgui + sdl_lcd + SDL2)
include(${CMAKE_CURRENT_LIST_DIR}/../ymgui_app.cmake)

# 实时监控面板(第七个基础验证项目)。
# 本项目不催生新控件——目标是把此前只有孤立 demo、从没进过真应用的一簇能力
# 一次性压出来:Tabview / Meter / ArcWidget / Bar / Chart / Spinner / Switch /
# Checkbox + 完整数据绑定线(UI=f(state))+ 布局助手 Stack/Align。
# 分层铁律:控件不认识 "CPU/温度";传感器语义、合成波形、告警阈值全在本 app 侧。
ymgui_add_app(dashboard
	dashboard.c)

# GB2312 全字库外部 blob 路径(库在 Demo/ 下生成,项目复用同一份)
target_compile_definitions(dashboard PRIVATE
	GB2312_BIN_PATH="${YMGUI_REPO_ROOT}/tools/gb2312_glyphs.bin")
