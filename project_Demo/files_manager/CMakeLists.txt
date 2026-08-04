cmake_minimum_required(VERSION 3.16)
project(files_manager C)

# 咬合层:把仓库里的 YMGUI/ 当外部依赖引入(建 ymgui + sdl_lcd + SDL2)
include(${CMAKE_CURRENT_LIST_DIR}/../ymgui_app.cmake)

# 文件管理器(第二个基础验证项目;树形浏览 + 文本预览 + 文件操作)
ymgui_add_app(files_manager files_manager.c)

# GB2312 全字库外部 blob 路径(库在 Demo/ 下生成,项目复用同一份)
target_compile_definitions(files_manager PRIVATE
	GB2312_BIN_PATH="${YMGUI_REPO_ROOT}/Demo/gb2312_glyphs.bin")
