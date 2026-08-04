cmake_minimum_required(VERSION 3.16)
project(image_edit C)

# 咬合层:把仓库里的 YMGUI/ 当外部依赖引入(建 ymgui + sdl_lcd + SDL2)
include(${CMAKE_CURRENT_LIST_DIR}/../ymgui_app.cmake)

# 类 PS 画板(第四个基础验证项目)。催生库控件 Canvas(可绘制位图视口)+ ColorPicker(HSV 取色)。
# 图层栈 / 融合 / 合成全在 app 侧(与库分层纪律一致:Canvas 不懂图层)。
ymgui_add_app(image_edit image_edit.c)

# GB2312 全字库外部 blob 路径(库在 Demo/ 下生成,项目复用同一份)
target_compile_definitions(image_edit PRIVATE
	GB2312_BIN_PATH="${YMGUI_REPO_ROOT}/Demo/gb2312_glyphs.bin")
