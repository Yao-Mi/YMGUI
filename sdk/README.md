# YMGUI 二进制 SDK

预编译静态库、配套头文件、CMake 接口、示例和中文字模组成的独立开发包。接入应用时无需编译 YMGUI 实现。使用方式参考 [手册](MANUAL.md)，源码示例在 [examples](examples)。

## 在源码仓库生成

从 YMGUI 仓库根目录执行：

```bash
./sdk/build.sh
# 可选：验证外部 YMGRE SDK 的所有窗口示例
./sdk/build.sh --ymgre-dir /path/to/YMGRE_libs/cmake
# 更新需要提交的分发包（仍先完成全部接入验证）
./sdk/build.sh --release --ymgre-dir /path/to/YMGRE_libs/cmake
```

需要 Python 3、CMake、C 编译器、binutils、make，以及 SDL2 开发包和 pkg-config。`--jobs 8` 调整并行数；`--without-sdl` 生成纯核心包，此时不需要 SDL2 或 pkg-config。当前打包器支持原生 Linux 构建，默认 Release、PIC、16 位坐标、完整默认控件配置。

生成位置为 `build/YMGUI_libs/`，压缩包为 `build/YMGUI_libs-linux-<架构>.tar.gz`，附带压缩包 SHA-256 文件。构建中间文件在 `build/_ymgui_sdk/`；包内不包含库实现、FFmpeg、SDL2 库或 project_Demo 应用。

`build/` 属于 Git 忽略目录，本地生成不等于已提交。加 `--release` 后，压缩包及校验文件输出到仓库可跟踪的 `releases/`；将这两个文件与相关改动一起提交，其他人才能直接取得预编译包。普通构建不更新该分发目录。

## 包内容

| 路径 | 用途 |
| --- | --- |
| `lib/libymgui_rgb16.a`、`libymgui_rgb24.a` | RGB565 / RGB888 核心库，依赖 C 运行库、libm 和动态加载接口 |
| `lib/libymgui_sdl_rgb16.a`、`libymgui_sdl_rgb24.a` | 可选 SDL 显示 / 输入适配层，依赖对应核心和外部 SDL2 |
| `include/YMGUI/`、`include/SDL_LCD/` | 和二进制匹配的头文件 |
| `cmake/YMGUIConfig.cmake` | 导出 `YMGUI::ymgui` 和可选 `YMGUI::sdl` |
| `examples/` | 不依赖 SDL 的绘图 / 输入 / 绑定示例，以及 SDL 按钮窗口示例 |
| `bin/rgb16/`、`bin/rgb24/` | 已构建的对应示例 |
| `resources/gb2312_glyphs.bin` | 外部中文字模，应用按需加载 |
| `BUILD_INFO.json`、`verification/` | 平台、源码摘要、配置和验收日志 |
| `checksums.sha256`、`LICENSE` | 文件完整性清单、仓库原有 Apache-2.0 许可 |

两种色深的库使用各自文件名，头文件通过导入目标传递的色深宏匹配。包只适用于构建时记录的平台 / 架构，MCU、Android、Windows 需使用对应工具链重新构建。

## 拿到包后开始使用

解压后进入 `YMGUI_libs`：

```bash
sha256sum -c checksums.sha256
./build_demos.sh 16
./examples-build16/demo_sdk_window
# 需要 RGB888 时用另一个构建目录
./build_demos.sh 24
```

`demo_sdk_minimal` 无需显示器，会自动注入一次按钮点击、验证绑定和空闲刷新，再向当前工作目录写入 `ymgui.ppm`。窗口示例不传参数时持续运行，传 `30` 可在 30 帧后退出。`build_demos.sh` 的 CTest 用 SDL dummy 完成有限帧验证。

自己的 CMake 工程只需：

```cmake
find_package(YMGUI CONFIG REQUIRED COMPONENTS core sdl)
add_executable(my_app main.c)
target_link_libraries(my_app PRIVATE YMGUI::sdl)
```

配置时传 `-DYMGUI_DIR=/absolute/path/YMGUI_libs/cmake -DYMGUI_COLOR_DEPTH=16`。纯核心应用只链接 `YMGUI::ymgui`，并可设 `-DYMGUI_WITH_SDL=OFF`，不需要 SDL 开发环境。详见 [手册](MANUAL.md)。
