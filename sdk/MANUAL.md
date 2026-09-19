# YMGUI SDK 使用手册

## 范围与模块

YMGUI 是 C99 + GNU 扩展的嵌入式 GUI：保留模式对象树、脏矩形和 band 分块刷新，支持控件、输入事件、状态绑定和插件。SDK 将这些实现预编译为静态库；SDL 适配单独链接。几何 / 字体 / 渲染 / 控件不需要窗口系统。

核心包含 Button、Label、Checkbox、Switch、Slider、Bar、Image、Arc、Spinner、Meter、TextInput、TextView、EditView、List、Chart、Dropdown、Table、Tabview、TreeView、Grid、Canvas、ColorPicker、Roller、BarChart、MsgBox、FileDialog、Joystick，以及 base 容器。应用负责图片 / 音视频解码、文件系统后端、电子表格公式、图层和业务线程。

## CMake 接入

```cmake
cmake_minimum_required(VERSION 3.16)
project(my_app C)
set(CMAKE_C_STANDARD 99)
find_package(YMGUI CONFIG REQUIRED COMPONENTS core)
add_executable(my_app main.c)
target_link_libraries(my_app PRIVATE YMGUI::ymgui)
```

```bash
cmake -S . -B build16 -DYMGUI_DIR=/path/to/YMGUI_libs/cmake -DYMGUI_COLOR_DEPTH=16 -DYMGUI_WITH_SDL=OFF
cmake --build build16
```

桌面应用改为 `COMPONENTS core sdl` 并链接 `YMGUI::sdl`，该目标自动传递核心库、SDL2、数学库和动态加载依赖。SDL2 通过系统 pkg-config 查找；未安装 SDL2、纯核心包或显式 `YMGUI_WITH_SDL=OFF` 时不会创建 SDL 目标，但仍能使用核心。默认在依赖可用时自动提供 SDL 目标，兼容 YMGRE 的外部 GUI 接口约定。

包可以整体移动；导入目标只从包内相对位置寻找库和头文件，不查找 YMGUI 仓库。一个 consumer 构建只能使用同一份 SDK、同一种色深；切换色深使用独立 build 目录。

## 配置与 ABI

- `YMGUI_COLOR_DEPTH=16` 为 RGB565；`24` 为 RGB888（三字节 GYpx）。CMake 自动选择相应核心 / SDL archive 并传递编译宏。
- 预编译包为默认完整配置、16 位 GYcoord、`GY_INV_MAX=16`、Release/PIC。包内 `YMGUI_SDK_Config.h` 会拒绝常见的色深、坐标宽度和脏区数组 ABI 冲突。
- 不要只修改包内头文件、`GY_INV_MAX`、结构定义或功能宏来定制预编译库。裁减功能、替换内存分配器、改变 ABI 或移植平台需从源码重建，再同时更新二进制和头文件。
- 当前发布器只验证原生 Linux；架构、指针位宽与构建系统记录在 `cmake/platform.cmake` / `BUILD_INFO.json`，CMake 会拒绝不匹配的目标。相同架构的旧系统仍可能因 glibc / SDL2 版本不兼容而需要重新构建。
- 动态插件也必须使用兼容的头文件与色深；通过宿主函数表访问 UI，卸载动态库前释放所有指向其回调的对象。

## 两条最小调用路径

### 无 SDL / 自定义 LCD

从 [demo_sdk_minimal.c](examples/demo_sdk_minimal.c) 开始。初始化 `GYdisp`（先清零）、提供像素 buffer 和 `flush_cb`，创建上下文和控件，将输入通过 `YMGUI_Inject_*` 注入，然后 `YMGUI_Refresh(ctx)`。同步 flush 在传输完成后调用 `YMGUI_Disp_FlushReady`；真实 DMA 在完成中断里通知，不能提前复用正在传输的 buffer。

这个示例保存 PPM 只是演示后端；实际 MCU 可把相同区域和像素发送给 LCD。但本包的 Linux 二进制不能用于 MCU，需为目标处理器重新编译。

### SDL 窗口

从 [demo_sdk_window.c](examples/demo_sdk_window.c) 开始。`SDL_LCD_Init` 负责窗口和 flush；主循环依次执行 `SDL_LCD_PumpEvents`、`YMGUI_Refresh` 和延时。退出先清理注入上下文、对象树，再销毁 SDL 和应用分配的 buffer。

`SDL_LCD_WindowId()` 返回当前 SDL 窗口 ID，未初始化或已销毁时为 0；`SDL_LCD_SetTitle(title)` 设置标题，成功返回 1，无窗口或空指针返回 0。接口不暴露 SDL 结构体类型，供外部引擎的输入桥接和窗口管理使用。

## 资源与生命周期技巧

- 父对象销毁会级联释放孩子；控件私有 `user_data` 不可覆盖成应用指针。
- Label / Button 文本会复制，但有容量限制；Image 的 `GYimg` 和像素为借用，应用保证存活。更换像素内容后标脏 Image。
- subject 和字符串缓冲需活得比绑定久；原地修改字符串后 `YMGUI_State_Touch`。双向绑定占用 changed 回调，额外逻辑用 subject observer。应用观察者要显式解除。
- 自绘的图元使用屏幕坐标，surface 仅包含当前 band。不要按全屏大小直接访问 band buffer。
- SDL 自动注入 Tick；裸机调用 `YMGUI_Inject_Tick` 并传实际经过毫秒。控件更新集中在 UI 线程。
- 内建 CJK 是精简字集。需要外部字模时读取 `resources/gb2312_glyphs.bin`，参照 `include/YMGUI/CORE/YMGUI_Font.h` 构造带 `glyph_read` 的 GYfont，使用库中的 `YMGUI_GB2312_cps` 索引与 `YMGUI_Font_SetFallback`。当前 blob 为 7672 字形，每字 128 字节；应用自己管理资源位置，SDK 不把构建机器路径写入库。
- 文件选择器需注入文件系统回调；SDK 不携带 FFmpeg、词典、project_Demo 或其他业务资源。

## YMGRE 联用

YMGRE 的窗口示例可直接链接 `YMGUI::ymgui` 和 `YMGUI::sdl`，应用侧 host 负责把渲染图像交给 Image。当前 YMGRE 二进制包使用 RGB565，因此联用选择 `YMGUI_COLOR_DEPTH=16`。

```bash
cmake -S /path/to/YMGRE_libs/examples -B gre-examples \
  -DYMGRE_DIR=/path/to/YMGRE_libs/cmake \
  -DYMGUI_DIR=/path/to/YMGUI_libs/cmake \
  -DYMGRE_WINDOWED_DEMOS=ON -DYMGRE_INDEX_BITS=16 -DYMGUI_COLOR_DEPTH=16
cmake --build gre-examples -j4
ctest --test-dir gre-examples --output-on-failure
```

YMGRE 和 YMGUI 保持各自的库包及许可；不将另一套引擎的库和头文件混入本包。

## 验收和完整性

生成器会把包复制到仓库外的临时位置，分别对两种色深执行纯核心 / SDL 示例构建和 CTest，检查核心 archive 无 SDL 未解析符号，并验证非法色深和 ABI 定义被拒绝。提供 `--ymgre-dir` 时额外构建运行该包的 index16 / index32 示例。只有通过的包才发布到 `build/YMGUI_libs`，原输出目录需带生成器标记才可覆盖。

`verification/` 保存执行结果；`BUILD_INFO.json` 记录本次配置和是否实际执行 YMGRE 验证，不承诺所有平台通过。解压后运行 `sha256sum -c checksums.sha256` 检查包内文件。文件哈希用于完整性检查，不代表发布者身份签名。

本包沿用 YMGUI 仓库的 Apache-2.0 `LICENSE`。SDL2、YMGRE 等外部依赖遵循各自许可，未把 YMGRE 的许可复制到 YMGUI。
