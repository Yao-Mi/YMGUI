cmake_minimum_required(VERSION 3.16)
project(YMGUI C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
# C99 + GNU 扩展
set(CMAKE_C_EXTENSIONS ON)

set(YMGUI_COLOR_DEPTH "16" CACHE STRING "Framebuffer color depth: 1, 8, 16 or 24 (RGB888)")
set_property(CACHE YMGUI_COLOR_DEPTH PROPERTY STRINGS 1 8 16 24)
if(NOT YMGUI_COLOR_DEPTH MATCHES "^(1|8|16|24)$")
	message(FATAL_ERROR "YMGUI_COLOR_DEPTH must be 1, 8, 16 or 24")
endif()
# GYpx is part of the public ABI, so every library and executable must use the same depth.
add_compile_definitions(YMGUI_COLOR_DEPTH=${YMGUI_COLOR_DEPTH})

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Debug)
endif()

# 库本体根目录:10 个可移植层全收在 YMGUI/ 下(对齐 YMCV 的 OpenSrc-YMCV/YMCV/)。
#   移植 = 把 YMGUI/ 整个拷进你的工程 + 改 CONFIG/ + 照 SDL_LCD/ 写真实 LCD 的 flush。
#   SDL_LCD/、Demo/ 是 PC 演示外壳,留在顶层,移植时不带走。
set(YMGUI_ROOT ${CMAKE_SOURCE_DIR}/YMGUI)

# 裸头式 include:把每个含头目录都加进搜索路径(全库头文件无重名,文件挪位置不断链)
include_directories(
	${YMGUI_ROOT}/CONFIG
	${YMGUI_ROOT}/DEBUG
	${YMGUI_ROOT}/COMMON/GEOM
	${YMGUI_ROOT}/COMMON/MATH
	${YMGUI_ROOT}/OPOBJ
	${YMGUI_ROOT}/CORE
	${YMGUI_ROOT}/GUI
	${YMGUI_ROOT}/WIDGET
	${YMGUI_ROOT}/STATE
	${YMGUI_ROOT}/PLUGIN
	${YMGUI_ROOT}/HAL
	${CMAKE_SOURCE_DIR}/SDL_LCD
)

# 库本体(平台无关 + HAL 接口),不含 SDL
file(GLOB YMGUI_SRC
	${YMGUI_ROOT}/CONFIG/*.c
	${YMGUI_ROOT}/DEBUG/*.c
	${YMGUI_ROOT}/COMMON/GEOM/*.c
	${YMGUI_ROOT}/COMMON/MATH/*.c
	${YMGUI_ROOT}/OPOBJ/*.c
	${YMGUI_ROOT}/CORE/*.c
	${YMGUI_ROOT}/GUI/*.c
	${YMGUI_ROOT}/WIDGET/*.c
	${YMGUI_ROOT}/STATE/*.c
	${YMGUI_ROOT}/PLUGIN/*.c
	${YMGUI_ROOT}/HAL/*.c
)
add_library(ymgui STATIC ${YMGUI_SRC})

# SDL_LCD port + demo(Linux 假 LCD)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

# 插件系统宿主 demo（静态注册路径；Linux 同时编译 dlopen 支持）
add_executable(demo_plugin
	${CMAKE_SOURCE_DIR}/SDL_LCD/SDL_LCD.c
	${CMAKE_SOURCE_DIR}/Demo/demo_plugin.c
)
target_include_directories(demo_plugin PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(demo_plugin ymgui ${SDL2_LIBRARIES} m ${CMAKE_DL_LIBS})

add_executable(demo_flush_band
	${CMAKE_SOURCE_DIR}/SDL_LCD/SDL_LCD.c
	${CMAKE_SOURCE_DIR}/Demo/demo_flush_band.c
)
target_include_directories(demo_flush_band PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(demo_flush_band ymgui ${SDL2_LIBRARIES} m)

# SDL 标题栏关闭请求回调 demo（首次取消，再次允许）
add_executable(demo_close_request
	${CMAKE_SOURCE_DIR}/SDL_LCD/SDL_LCD.c
	${CMAKE_SOURCE_DIR}/Demo/demo_close_request.c
)
target_include_directories(demo_close_request PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(demo_close_request ymgui ${SDL2_LIBRARIES} m)

# 无 SDL 的像素正确性单测
add_executable(test_fill ${CMAKE_SOURCE_DIR}/tests/test_fill.c)
target_link_libraries(test_fill ymgui m)

# 无 SDL 的交互闭环单测
add_executable(test_event ${CMAKE_SOURCE_DIR}/tests/test_event.c)
target_link_libraries(test_event ymgui m)

# SDL 输入映射与窗口关闭请求单测
add_executable(test_sdl_context
	${CMAKE_SOURCE_DIR}/SDL_LCD/SDL_LCD.c
	${CMAKE_SOURCE_DIR}/tests/test_sdl_context.c
)
target_include_directories(test_sdl_context PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(test_sdl_context ymgui ${SDL2_LIBRARIES} m)

# 无 SDL 的多矩形脏区单测
add_executable(test_invalidate ${CMAKE_SOURCE_DIR}/tests/test_invalidate.c)
target_link_libraries(test_invalidate ymgui m)

# 无 SDL 的位图字体单测
add_executable(test_font ${CMAKE_SOURCE_DIR}/tests/test_font.c)
target_link_libraries(test_font ymgui m)

# 无 SDL 的控件文字单测
add_executable(test_widget ${CMAKE_SOURCE_DIR}/tests/test_widget.c)
target_link_libraries(test_widget ymgui m)

# 无 SDL 的表单四件套单测
add_executable(test_form ${CMAKE_SOURCE_DIR}/tests/test_form.c)
target_link_libraries(test_form ymgui m)

# 无 SDL 的图元单测
add_executable(test_draw ${CMAKE_SOURCE_DIR}/tests/test_draw.c)
target_link_libraries(test_draw ymgui m)

# 无 SDL 的显示控件单测
add_executable(test_display ${CMAKE_SOURCE_DIR}/tests/test_display.c)
target_link_libraries(test_display ymgui m)

# 无 SDL 的粗弧无缝隙单测
add_executable(test_arc ${CMAKE_SOURCE_DIR}/tests/test_arc.c)
target_link_libraries(test_arc ymgui m)

# 无 SDL 的文本输入单测
add_executable(test_textinput ${CMAKE_SOURCE_DIR}/tests/test_textinput.c)
target_link_libraries(test_textinput ymgui m)

# 无 SDL 的 Tab 焦点轮转单测
add_executable(test_focus ${CMAKE_SOURCE_DIR}/tests/test_focus.c)
target_link_libraries(test_focus ymgui m)

# 无 SDL 的分块刷新回归单测
add_executable(test_band ${CMAKE_SOURCE_DIR}/tests/test_band.c)
target_link_libraries(test_band ymgui m)

# 无 SDL 的滚动+子裁剪机制单测
add_executable(test_scroll ${CMAKE_SOURCE_DIR}/tests/test_scroll.c)
target_link_libraries(test_scroll ymgui m)

# 无 SDL 的可滚动列表单测
add_executable(test_list ${CMAKE_SOURCE_DIR}/tests/test_list.c)
target_link_libraries(test_list ymgui m)

# 无 SDL 的折线图单测
add_executable(test_chart ${CMAKE_SOURCE_DIR}/tests/test_chart.c)
target_link_libraries(test_chart ymgui m)

# 无 SDL 的下拉框 + top_layer 弹出层单测
add_executable(test_dropdown ${CMAKE_SOURCE_DIR}/tests/test_dropdown.c)
target_link_libraries(test_dropdown ymgui m)

# 无 SDL 的表格单测
add_executable(test_table ${CMAKE_SOURCE_DIR}/tests/test_table.c)
target_link_libraries(test_table ymgui m)

# 无 SDL 的标签页单测
add_executable(test_tabview ${CMAKE_SOURCE_DIR}/tests/test_tabview.c)
target_link_libraries(test_tabview ymgui m)

# 无 SDL 的状态/数据绑定单测
add_executable(test_bind ${CMAKE_SOURCE_DIR}/tests/test_bind.c)
target_link_libraries(test_bind ymgui m)

add_executable(test_aa ${CMAKE_SOURCE_DIR}/tests/test_aa.c)
target_link_libraries(test_aa ymgui m)

# 无 SDL 的异步双缓冲时序单测(mock DMA:ping-pong 交替 + 在途<=1 + 单缓冲退化)
add_executable(test_async ${CMAKE_SOURCE_DIR}/tests/test_async.c)
target_link_libraries(test_async ymgui m)

add_executable(test_layout ${CMAKE_SOURCE_DIR}/tests/test_layout.c)
target_link_libraries(test_layout ymgui m)

# 无 SDL 的多行文本视图单测(断行/折行/滚动钳制/宽度重排)
add_executable(test_textview ${CMAKE_SOURCE_DIR}/tests/test_textview.c)
target_link_libraries(test_textview ymgui m)

# 无 SDL 的可编辑多行文本框单测(UTF-8 插入/换行/跨行退格/方向键/折行/滚动钳制)
add_executable(test_editview ${CMAKE_SOURCE_DIR}/tests/test_editview.c)
target_link_libraries(test_editview ymgui m)

# 无 SDL 的树形视图单测(节点增删/展开重建/懒加载/点击选中/双击激活/滚动钳制/ClearChildren)
add_executable(test_treeview ${CMAKE_SOURCE_DIR}/tests/test_treeview.c)
target_link_libraries(test_treeview ymgui m)

# 无 SDL 的网格单测(单元格存取/单击选中/双击编辑/SetSelected/二维滚动钳位/EnsureVisible/方向键/析构)
add_executable(test_grid ${CMAKE_SOURCE_DIR}/tests/test_grid.c)
target_link_libraries(test_grid ymgui m)

add_executable(test_canvas ${CMAKE_SOURCE_DIR}/tests/test_canvas.c)
target_link_libraries(test_canvas ymgui m)

add_executable(test_colorpicker ${CMAKE_SOURCE_DIR}/tests/test_colorpicker.c)
target_link_libraries(test_colorpicker ymgui m)

add_executable(test_joystick ${CMAKE_SOURCE_DIR}/tests/test_joystick.c)
target_link_libraries(test_joystick ymgui m)

add_executable(test_roller ${CMAKE_SOURCE_DIR}/tests/test_roller.c)
target_link_libraries(test_roller ymgui m)

add_executable(test_barchart ${CMAKE_SOURCE_DIR}/tests/test_barchart.c)
target_link_libraries(test_barchart ymgui m)

# 无 SDL 的模态对话框单测(输入模态锁死/吞底层点击/按钮回调+隐藏/解锁后可点/多按钮 index/CtxFree 兜底不崩)
add_executable(test_msgbox ${CMAKE_SOURCE_DIR}/tests/test_msgbox.c)
target_link_libraries(test_msgbox ymgui m)

# 无 SDL 的文件对话框单测(三模式/导航/容量/新建目录/覆盖确认,虚拟文件系统回调)
add_executable(test_filedialog ${CMAKE_SOURCE_DIR}/tests/test_filedialog.c)
target_link_libraries(test_filedialog ymgui m)

# 插件注册表生命周期单测（注册/失败回滚/卸载重载/容量边界）
add_executable(test_plugin ${CMAKE_SOURCE_DIR}/tests/test_plugin.c)
target_link_libraries(test_plugin ymgui m ${CMAKE_DL_LIBS})

# 表单四件套交互 demo
add_executable(demo_form
	${CMAKE_SOURCE_DIR}/SDL_LCD/SDL_LCD.c
	${CMAKE_SOURCE_DIR}/Demo/demo_form.c
)
target_include_directories(demo_form PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(demo_form ymgui ${SDL2_LIBRARIES} m)

# 图元展示 demo
add_executable(demo_draw
	${CMAKE_SOURCE_DIR}/SDL_LCD/SDL_LCD.c
	${CMAKE_SOURCE_DIR}/Demo/demo_draw.c
)
target_include_directories(demo_draw PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(demo_draw ymgui ${SDL2_LIBRARIES} m)

# 图片控件缩放模式 demo(NONE/FIT/FILL,库缺口 Draw_ImgScaled 落地)
add_executable(demo_image
	${CMAKE_SOURCE_DIR}/SDL_LCD/SDL_LCD.c
	${CMAKE_SOURCE_DIR}/Demo/demo_image.c
)
target_include_directories(demo_image PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(demo_image ymgui ${SDL2_LIBRARIES} m)

# 显示控件仪表盘 demo
add_executable(demo_dashboard
	${CMAKE_SOURCE_DIR}/SDL_LCD/SDL_LCD.c
	${CMAKE_SOURCE_DIR}/Demo/demo_dashboard.c
)
target_include_directories(demo_dashboard PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(demo_dashboard ymgui ${SDL2_LIBRARIES} m)

# 文本输入 demo
add_executable(demo_textinput
	${CMAKE_SOURCE_DIR}/SDL_LCD/SDL_LCD.c
	${CMAKE_SOURCE_DIR}/Demo/demo_textinput.c
)
target_include_directories(demo_textinput PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(demo_textinput ymgui ${SDL2_LIBRARIES} m)

# 可滚动列表 demo
add_executable(demo_list
	${CMAKE_SOURCE_DIR}/SDL_LCD/SDL_LCD.c
	${CMAKE_SOURCE_DIR}/Demo/demo_list.c
)
target_include_directories(demo_list PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(demo_list ymgui ${SDL2_LIBRARIES} m)

# 折线图流式滚动 demo
add_executable(demo_chart
	${CMAKE_SOURCE_DIR}/SDL_LCD/SDL_LCD.c
	${CMAKE_SOURCE_DIR}/Demo/demo_chart.c
)
target_include_directories(demo_chart PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(demo_chart ymgui ${SDL2_LIBRARIES} m)

# 下拉框浮动菜单 demo
add_executable(demo_dropdown
	${CMAKE_SOURCE_DIR}/SDL_LCD/SDL_LCD.c
	${CMAKE_SOURCE_DIR}/Demo/demo_dropdown.c
)
target_include_directories(demo_dropdown PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(demo_dropdown ymgui ${SDL2_LIBRARIES} m)

# 表格 demo
add_executable(demo_table
	${CMAKE_SOURCE_DIR}/SDL_LCD/SDL_LCD.c
	${CMAKE_SOURCE_DIR}/Demo/demo_table.c
)
target_include_directories(demo_table PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(demo_table ymgui ${SDL2_LIBRARIES} m)

# 标签页 demo
add_executable(demo_tabview
	${CMAKE_SOURCE_DIR}/SDL_LCD/SDL_LCD.c
	${CMAKE_SOURCE_DIR}/Demo/demo_tabview.c
)
target_include_directories(demo_tabview PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(demo_tabview ymgui ${SDL2_LIBRARIES} m)

# 画布控件 demo(可绘制位图 + 放大显示)
add_executable(demo_canvas
	${CMAKE_SOURCE_DIR}/SDL_LCD/SDL_LCD.c
	${CMAKE_SOURCE_DIR}/Demo/demo_canvas.c
)
target_include_directories(demo_canvas PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(demo_canvas ymgui ${SDL2_LIBRARIES} m)

# 虚拟摇杆 demo(鼠标/触摸拖动 + 圆形限位 + 松手回中 + 标准化输出)
add_executable(demo_joystick
	${CMAKE_SOURCE_DIR}/SDL_LCD/SDL_LCD.c
	${CMAKE_SOURCE_DIR}/Demo/demo_joystick.c
)
target_include_directories(demo_joystick PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(demo_joystick ymgui ${SDL2_LIBRARIES} m)

# HSV 取色器 demo
add_executable(demo_colorpicker
	${CMAKE_SOURCE_DIR}/SDL_LCD/SDL_LCD.c
	${CMAKE_SOURCE_DIR}/Demo/demo_colorpicker.c
)
target_include_directories(demo_colorpicker PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(demo_colorpicker ymgui ${SDL2_LIBRARIES} m)

# 数据绑定 demo(UI=f(state),无胶水回调)
add_executable(demo_bind
	${CMAKE_SOURCE_DIR}/SDL_LCD/SDL_LCD.c
	${CMAKE_SOURCE_DIR}/Demo/demo_bind.c
)
target_include_directories(demo_bind PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(demo_bind ymgui ${SDL2_LIBRARIES} m)

# 布局 demo(SDL 假 LCD,Stack/Align 排布设置面板)
add_executable(demo_layout
	${CMAKE_SOURCE_DIR}/SDL_LCD/SDL_LCD.c
	${CMAKE_SOURCE_DIR}/Demo/demo_layout.c
)
target_include_directories(demo_layout PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(demo_layout ymgui ${SDL2_LIBRARIES} m)

# 多行文本视图 demo(拖动滚动 + Wrap 开关)
add_executable(demo_textview
	${CMAKE_SOURCE_DIR}/SDL_LCD/SDL_LCD.c
	${CMAKE_SOURCE_DIR}/Demo/demo_textview.c
)
target_include_directories(demo_textview PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(demo_textview ymgui ${SDL2_LIBRARIES} m)

# 树形视图 demo(单击标记展开/收起 + 懒加载 + 拖动滚动 + 单击选中/双击打开)
add_executable(demo_treeview
	${CMAKE_SOURCE_DIR}/SDL_LCD/SDL_LCD.c
	${CMAKE_SOURCE_DIR}/Demo/demo_treeview.c
)
target_include_directories(demo_treeview PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(demo_treeview ymgui ${SDL2_LIBRARIES} m)

# 滚轮/居中高亮平滑滚动列表 demo(交互拖动吸附 + 程序缓动到位)
add_executable(demo_roller
	${CMAKE_SOURCE_DIR}/SDL_LCD/SDL_LCD.c
	${CMAKE_SOURCE_DIR}/Demo/demo_roller.c
)
target_include_directories(demo_roller PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(demo_roller ymgui ${SDL2_LIBRARIES} m)

# 通用柱状图 demo(流式喂值 + 顶部回落 + 配色/顶标四组合轮换)
add_executable(demo_barchart
	${CMAKE_SOURCE_DIR}/SDL_LCD/SDL_LCD.c
	${CMAKE_SOURCE_DIR}/Demo/demo_barchart.c
)
target_include_directories(demo_barchart PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(demo_barchart ymgui ${SDL2_LIBRARIES} m)

# 可编辑多行文本框 demo(真键盘编辑 + 中文输入 + Wrap/Clear;GB2312 全集走外部 flash blob)
add_executable(demo_editview
	${CMAKE_SOURCE_DIR}/SDL_LCD/SDL_LCD.c
	${CMAKE_SOURCE_DIR}/Demo/demo_editview.c
)
target_include_directories(demo_editview PRIVATE ${SDL2_INCLUDE_DIRS})
target_compile_definitions(demo_editview PRIVATE
	GB2312_BIN_PATH="${CMAKE_SOURCE_DIR}/tools/gb2312_glyphs.bin")
target_link_libraries(demo_editview ymgui ${SDL2_LIBRARIES} m)

# 模态对话框 demo(SDL 假 LCD,按钮弹阻塞模态,遮罩锁底层)
add_executable(demo_msgbox
	${CMAKE_SOURCE_DIR}/SDL_LCD/SDL_LCD.c
	${CMAKE_SOURCE_DIR}/Demo/demo_msgbox.c
)
target_include_directories(demo_msgbox PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(demo_msgbox ymgui ${SDL2_LIBRARIES} m)

# 文件对话框 demo(SDL + POSIX 文件系统适配层;库本体仍不依赖 POSIX)
add_executable(demo_filedialog
	${CMAKE_SOURCE_DIR}/SDL_LCD/SDL_LCD.c
	${CMAKE_SOURCE_DIR}/Demo/demo_filedialog.c
)
target_include_directories(demo_filedialog PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(demo_filedialog ymgui ${SDL2_LIBRARIES} m)

# 交互 demo(SDL 假 LCD,鼠标点击按钮)
add_executable(demo_button
	${CMAKE_SOURCE_DIR}/SDL_LCD/SDL_LCD.c
	${CMAKE_SOURCE_DIR}/Demo/demo_button.c
)
target_include_directories(demo_button PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(demo_button ymgui ${SDL2_LIBRARIES} m)

# 中文字体 demo(默认字体→CJK 回退链,标签/按钮直接塞 UTF-8 中文)
add_executable(demo_font_cjk
	${CMAKE_SOURCE_DIR}/SDL_LCD/SDL_LCD.c
	${CMAKE_SOURCE_DIR}/Demo/demo_font_cjk.c
)
target_include_directories(demo_font_cjk PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(demo_font_cjk ymgui ${SDL2_LIBRARIES} m)

# 方案3+外部flash demo(GB2312 全集字模放磁盘 blob 模拟外部 flash,glyph_read 回调取字)
add_executable(demo_font_gb2312
	${CMAKE_SOURCE_DIR}/SDL_LCD/SDL_LCD.c
	${CMAKE_SOURCE_DIR}/Demo/demo_font_gb2312.c
)
target_include_directories(demo_font_gb2312 PRIVATE ${SDL2_INCLUDE_DIRS})
target_compile_definitions(demo_font_gb2312 PRIVATE
	GB2312_BIN_PATH="${CMAKE_SOURCE_DIR}/tools/gb2312_glyphs.bin")
target_link_libraries(demo_font_gb2312 ymgui ${SDL2_LIBRARIES} m)

enable_testing()
add_test(NAME test_fill COMMAND test_fill)
add_test(NAME test_event COMMAND test_event)
add_test(NAME test_sdl_context COMMAND test_sdl_context)
set_tests_properties(test_sdl_context PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy")
add_test(NAME test_invalidate COMMAND test_invalidate)
add_test(NAME test_font COMMAND test_font)
add_test(NAME test_widget COMMAND test_widget)
add_test(NAME test_form COMMAND test_form)
add_test(NAME test_draw COMMAND test_draw)
add_test(NAME test_display COMMAND test_display)
add_test(NAME test_arc COMMAND test_arc)
add_test(NAME test_textinput COMMAND test_textinput)
add_test(NAME test_focus COMMAND test_focus)
add_test(NAME test_band COMMAND test_band)
add_test(NAME test_scroll COMMAND test_scroll)
add_test(NAME test_list COMMAND test_list)
add_test(NAME test_chart COMMAND test_chart)
add_test(NAME test_dropdown COMMAND test_dropdown)
add_test(NAME test_table COMMAND test_table)
add_test(NAME test_tabview COMMAND test_tabview)
add_test(NAME test_bind COMMAND test_bind)
add_test(NAME test_aa COMMAND test_aa)
add_test(NAME test_async COMMAND test_async)
add_test(NAME test_layout COMMAND test_layout)
add_test(NAME test_textview COMMAND test_textview)
add_test(NAME test_editview COMMAND test_editview)
add_test(NAME test_treeview COMMAND test_treeview)
add_test(NAME test_grid COMMAND test_grid)
add_test(NAME test_canvas COMMAND test_canvas)
add_test(NAME test_colorpicker COMMAND test_colorpicker)
add_test(NAME test_joystick COMMAND test_joystick)
add_test(NAME test_roller COMMAND test_roller)
add_test(NAME test_barchart COMMAND test_barchart)
add_test(NAME test_msgbox COMMAND test_msgbox)
add_test(NAME test_filedialog COMMAND test_filedialog)
add_test(NAME test_plugin COMMAND test_plugin)
