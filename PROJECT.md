# YMGUI 项目记录文档

## 一、项目是什么

YMGUI 是一个**面向嵌入式/裸机优先**的跨平台 GUI 库，用 C99 + GNU 扩展编写。

一句话定位：**软件光栅化 + 保留模式 + 分块刷新的嵌入式 GUI 库**，桌面(Linux/SDL)开发、裸机(MCU + LCD)部署，两者共用同一套渲染代码，只换一个显示驱动回调。

## 二、为什么这么做（核心约束）

第一约束是**可移植到嵌入式**——这条约束倒推出了整个架构。工程约定大量继承自同一作者的早期项目 YMCV（图像处理库，`/home/yaomi/YMCV/ymcv_v2/`），因为二者撞了同一个约束。

裸机的现实决定了几个不可动摇的前提：
- LCD 只有两个原语：画点、blit 一块连续像素 → 库必须自己做软件光栅化。
- RAM 紧张 → draw buffer 可远小于整屏，分块(band)渲染。
- 常无 FPU → 数值以整数/定点为主，不走 float 必经路径。
- 无 OS 消息泵 → 输入靠主循环拉取式注入。
- Flash 有限 → 模块可裁剪。

## 三、锁定的地基决策（2026-07 起）

| 维度 | 决策 |
|------|------|
| 渲染 | 全自绘软件光栅化（不包原生控件、不依赖 GPU） |
| UI 范式 | 保留模式（对象树持久存在）+ 即时逃生口 |
| 刷新 | 分块(band)刷新，draw buffer 可 < 整屏 |
| 像素格式 | RGB888 / RGB565 / 8bit 灰度 / 1bpp（编译期 `YMGUI_COLOR_DEPTH` 切换） |
| 数值 | 定点/整数为主，`GYcoord=int16`，`GYvalue=16.16定点` |
| 最窄腰部 | 显示驱动回调 `flush(area, buf)` + 输入注入 |
| 参考 | LVGL 架构路线 |

这些决策的详细论证见 `ARCHITECTURE.md`。

## 四、当前进度快照

**代码规模**：约 1.4 万行库代码（不含 Demo/测试），48 个 `.c` 源文件（.c+.h 共 96）。测试源码集中在 `tests/`，独立于 `Demo/` 示例。

**已完成控件（27 个，含 base 容器）**：
- 基础：base 容器、Button、Label
- 表单：Checkbox、Switch、Slider、Bar(进度条)
- 显示：Image、Arc(环形进度)、Spinner(加载转圈)、Meter(仪表盘)、Chart(折线图)
- 输入：TextInput(单行文本输入框,UTF-8/自动水平视口/选区/剪贴板,容量创建期指定)、EditView(可编辑多行文本框,光标/换行/中文/选区/撤销/查找替换,容量创建期按需申报,索引全 size_t,撤销可关省 RAM,可设底色/边框)、Joystick(二维虚拟摇杆,-100..100 圆形输出/死区/可选回中/自定义绘制,`YMGUI_JOYSTICK` 可裁)
- 文本：TextView(只读多行文本视图,可滚动,Multiline/Wrap 可选)
- 容器：List(可滚动列表)、Table(表格)、Tabview(标签页)、Dropdown(下拉/弹出层)、TreeView(树形视图,展开/收起+缩进标记+懒加载,`YMGUI_TREEVIEW` 可裁)、Grid(可编辑单元格网格,二维滚动/选区/合并/插删行列,`YMGUI_GRID` 可裁)、FileDialog(三模式文件对话框,文件系统回调注入,`YMGUI_FILEDIALOG` 可裁)
- 绘图:Canvas(可绘制位图视口,自持 GYpx 缓冲直写/整数倍缩放/平移/绘制回调/喷枪逐帧驱动,`YMGUI_CANVAS` 可裁)、ColorPicker(HSV 取色器,SV 方块+色相条,整数 HSV↔RGB 无 FPU,`YMGUI_COLORPICKER` 可裁)
- 动效/数据可视化:Roller(居中高亮平滑滚动列表,歌词/时间/日历通用,程序驱动+可选拖动交互,`YMGUI_ROLLER` 可裁)、BarChart(通用柱状图,频谱/直方图/电平表/统计柱图通用,配色 BY_HEIGHT/PER_BAR × 顶标 BAR/NONE,值由 app 喂,`YMGUI_BARCHART` 可裁)

**图元层**：DrawFill(矩形)、DrawText(位图字体，UTF-8/CJK 回退链)、DrawLine(Bresenham + Wu 抗锯齿)、DrawImg(blit+colorkey)、DrawArc(画圆/圆盘/圆弧/粗弧环，距离场 AA)。

**事件/交互**：普通指针注入(按下/抬起/拖动/取消)、命中测试、pressed/clicked/pressing 语义、键盘注入、焦点系统；平台无关上下文输入(`ContextRequested/Dragging/Released/Cancelled`)，右键短点击与触摸长按统一、上下文拖动独立捕获且不产生普通 Clicked；失效/脏矩形(多矩形列表)、分块刷新管线、滚动+子裁剪机制、top_layer 弹出层。

**可裁减特性**：抗锯齿(`YMGUI_ANTIALIAS`，4bpp 灰度字体 + Wu 斜线 + 距离场圆弧，单色屏强制关)、中文/CJK(`YMGUI_FONT_CJK`，UTF-8 解码 + 稀疏排序码点表 + 外部 flash `glyph_read` 回调 + 运行期全局兜底钩子)、状态/数据绑定地基。另有轻量插件系统：裸机静态注册；Linux/Android 用 `dlopen`，Windows 用 `LoadLibrary`，可通过 `YMGUI_PLUGIN_DYNAMIC=0` 裁掉动态加载路径。

**中文字体三轴决策**：①CJK 开/关=编译期宏 `YMGUI_FONT_CJK`；②字集范围(方案1 精简 ~75字/9.6KB · 方案3 GB2312 6763字/~866KB)=字模**生成期** `gen_font.py` 参数(非宏)；③字模存放(内部 rodata / 外部 flash)=**运行期** `GYfont.glyph_read` 回调。方案切换不改任何控件代码，详见 `API.md` 表格。

**测试**：34 个 CTest（33 个无 SDL + 1 个 SDL dummy 输入映射测试），全部通过；26 个 SDL 交互 demo；10 个 project_Demo 完整应用(txt_edit、files_manager、excel_edit、image_edit、music_player、video_player、dashboard、alarm_clock、context_gesture、plugin_host)。`plugin_host` 是 800×480 插件管理器，包含插件清单、状态、加载/卸载、活动日志和由动态插件自建的内容页；验证 Linux `.so` 实际交互，以及 Windows `.dll`、Android arm64 `.so` 交叉构建。

**验证环境**：Ubuntu 22.04 + gcc 11.4 + cmake 3.22 + SDL2 2.0.20。

## 五、目录结构

刻意对齐 YMCV，让熟悉 YMCV 的人一眼知道怎么移植。角色相同的层用**一字不差**的名字，GUI 独有的层用同样风格新增。**可移植的 10 个层全收在 `YMGUI/` 库根下（对齐 YMCV 的 `OpenSrc-YMCV/YMCV/`），PC 演示外壳留在顶层**——移植时只拷 `YMGUI/` 一个文件夹。

```
YMGUI/          ★库本体,自包含,移植时整个拷走
  CONFIG/   平台适配 + 编译期配置（移植主战场）
  DEBUG/    assert/log 横切层
  COMMON/   平台无关纯算法（GEOM 几何 / MATH 定点三角）
  OPOBJ/    对象生命周期（Creat/Free 成对 + 树级联）
  CORE/     软件光栅化层（碰 GYsurface 像素的全在此）
  GUI/      ★GUI 机器（失效/脏矩形/事件/焦点）—— YMCV 无对应
  HAL/      显示驱动 + 输入注入接口（纯头，最窄腰部）
  WIDGET/   控件
  STATE/    状态/数据绑定（UI=f(state)）
  PLUGIN/   插件注册表 + 可选 OS 动态加载器（裸机走静态注册）
── 以下是 PC 演示外壳,移植时不带走 ──
SDL_LCD/  HAL 的 SDL 实现（Linux/Windows/Android 假 LCD，鼠标/触摸/文本输入桥接）
Demo/     示例 + 单测 + 字体/三角表生成脚本 + GB2312 字模 blob
CMakeLists.txt  桌面构建(GLOB YMGUI/ 各层 + SDL demo)
```

移植到真实硬件 = **把 `YMGUI/` 整个拷进你的工程** + 改 `YMGUI/CONFIG/` + 照着 `SDL_LCD/` 写一个真实 LCD 的 `flush_cb`。

## 六、如何构建与测试

```bash
cd /home/yaomi/YMGUI
cmake -B build -S .
cmake --build build

# 跑全部单测
cd build && ctest --output-on-failure

# 跑交互 demo（需显示器；headless 可加 SDL_VIDEODRIVER=dummy + 帧数参数）
./build/demo_dashboard     # 仪表盘/环形/转圈
./build/demo_form          # 表单四件套
./build/demo_list          # 可滚动列表
./build/demo_textinput     # 文本输入
./build/demo_chart         # 折线图流式滚动
./build/demo_table         # 表格
./build/demo_tabview       # 标签页
./build/demo_dropdown      # 下拉/弹出层
./build/demo_treeview      # 树形视图(展开/收起 + 懒加载 + 拖动滚动)
./build/demo_font_cjk      # 中文字体(方案1 精简集,字模内嵌)
./build/demo_font_gb2312   # 中文字体(方案3 GB2312 全集,走外部 flash 回调)
./build/demo_draw          # 图元展示
./build/demo_button        # 按钮
./build/project_Demo/context_gesture/context_gesture # 右键/长按菜单 + 上下文拖动 + 实时事件流
```

方案3 demo 的 GB2312 字模放在 `Demo/gb2312_glyphs.bin`（865664 B = 6763×128，模拟外部 flash 内容），由 `Demo/gen_font.py --cjk --gb2312 --extern --bin` 生成；固件内只驻留码点索引 `CORE/YMGUI_FontDataGB2312.c`（~13.5KB，无 bitmap）。

依赖：`sudo apt-get install libsdl2-dev`（仅桌面开发/demo 需要，库本体不依赖 SDL）。

## 七、相关文档

- `代码风格.md` — 命名/格式/目录归位/性能惯用手法（权威规范）
- `ARCHITECTURE.md` — 分层、核心类型、渲染管线、各机制设计
- `DEVLOG.md` — 逐轮开发进程 + 踩过的坑
- `CROSS_PLATFORM_PORTING.md` — Linux/Windows/Android SDL 桥接、鼠标/触摸输入、资源与构建验证；真实 MCU 仍照 `flush_cb` 最窄腰部实现
