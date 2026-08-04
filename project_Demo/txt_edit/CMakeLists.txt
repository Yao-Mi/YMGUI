cmake_minimum_required(VERSION 3.16)
project(txt_edit C)

# 咬合层:把仓库里的 YMGUI/ 当外部依赖引入(建 ymgui + sdl_lcd + SDL2)
include(${CMAKE_CURRENT_LIST_DIR}/../ymgui_app.cmake)

# 行式文本编辑器(第一个基础验证项目)
ymgui_add_app(txt_edit txt_edit.c)

# GB2312 全字库外部 blob 路径(库在 Demo/ 下生成,项目复用同一份)
target_compile_definitions(txt_edit PRIVATE
	GB2312_BIN_PATH="${YMGUI_REPO_ROOT}/Demo/gb2312_glyphs.bin")
