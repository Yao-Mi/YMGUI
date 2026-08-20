cmake_minimum_required(VERSION 3.16)
project(context_gesture C)

# 咬合层:把仓库里的 YMGUI/ 当外部依赖引入(建 ymgui + sdl_lcd + SDL2)
include(${CMAKE_CURRENT_LIST_DIR}/../ymgui_app.cmake)

# 不新增控件:用基础 GYOBJ、自定义 draw/event 回调和 top_layer 菜单
# 验证 SDL 右键/触摸长按到 YMGUI 上下文事件的完整链路。
ymgui_add_app(context_gesture
	context_gesture.c)
