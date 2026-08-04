cmake_minimum_required(VERSION 3.16)
project(excel_edit C)

# 咬合层:把仓库里的 YMGUI/ 当外部依赖引入(建 ymgui + sdl_lcd + SDL2)
include(${CMAKE_CURRENT_LIST_DIR}/../ymgui_app.cmake)

# 电子表格(第三个基础验证项目;催生库控件 Grid:单元格网格 + 二维滚动 + 行列表头)
# 公式引擎(tokenizer + 递归下降 parser + 引用 + 区间函数 + 定点)在 app 侧。
ymgui_add_app(excel_edit excel_edit.c)

# GB2312 全字库外部 blob 路径(库在 Demo/ 下生成,项目复用同一份)
target_compile_definitions(excel_edit PRIVATE
	GB2312_BIN_PATH="${YMGUI_REPO_ROOT}/Demo/gb2312_glyphs.bin")
