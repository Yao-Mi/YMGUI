# YMGUI：给 AI 编程助手的使用指南

> 核对日期：2026-09-10。用于快速选型、写应用和定位问题；完整能力以 [当前状态](当前状态.md) 为准，函数签名以仓库头文件为准。本文不替代 [仓库维护约定](../AGENTS.md)、[API 速查](API.md) 和测试中的行为规格。

## 1. 接手任务时先做什么

1. 阅读根 [README](../README.md) 和 [当前状态](当前状态.md)，再用本文定位相关模块。先检查 `git status --short`，识别已有改动。
2. 明确任务属于库核心、平台适配还是具体应用。只读对应头文件、实现和示例，不必通读所有源码或第三方依赖。
3. 查到真实函数再调用。YMGUI 使用 `Creat` 的既有拼写，例如 `YMGUI_Creat_Button_Creat`；不要按其他 GUI 框架的习惯猜 API。
4. 按改动范围构建、运行和验证，最后同步文档。需要承接开发背景时读 [DEVLOG V2](DEVLOG_V2.md)；`docs/history/` 只在追溯历史或回归时读。

事实优先级：代码 / 测试 / CMake → 当前状态 → 专题文档（含本文）→ DEVLOG → 历史归档。遇到冲突先核对实现，再修正文档。

## 2. 库的定位和目录分工

YMGUI 是嵌入式 / 裸机优先的保留模式 GUI：C99 + GNU 扩展、软件光栅化、对象树、脏矩形和分块刷新。桌面端以 SDL 模拟 LCD；核心不依赖 SDL、GPU 或 OS 消息泵。视频剪辑器等应用可以有独立的语言、平台和外部依赖要求。

| 目录 | 放什么 / 什么时候读 |
| --- | --- |
| `YMGUI/CONFIG/` | 公共类型、像素格式、功能宏、内存适配；改色深或移植内存时读 |
| `YMGUI/DEBUG/` | 断言和日志；错误处理时读 |
| `YMGUI/COMMON/` | 纯几何、Q15 三角查表；不依赖控件的算法 |
| `YMGUI/OPOBJ/` | 对象树、上下文、创建 / 释放、顶层容器 |
| `YMGUI/CORE/` | 对 surface 画填充、文字、线、图片、圆弧；字体数据 |
| `YMGUI/GUI/` | 失效、刷新、事件、焦点、一次性布局 |
| `YMGUI/HAL/` | 显示、输入、Tick、剪贴板的接口 |
| `YMGUI/WIDGET/` | 控件及其私有数据和回调 |
| `YMGUI/STATE/` | subject、observer 和控件绑定 |
| `YMGUI/PLUGIN/` | 静态插件注册、生命周期和可选动态加载 |
| `SDL_LCD/` | 桌面 / Android 的显示和输入桥接 |
| `sdk/` | 独立 lib 包的构建、CMake 接入、最小示例与手册；产物在 `build/YMGUI_libs/` |
| `releases/` | 随仓库提交的压缩包和校验文件；用 `./sdk/build.sh --release` 更新，勿误认为 `build/` 产物已经入库 |
| `Demo/` | 单控件和机制的用法示例，由根 CMake 构建 |
| `project_Demo/` | 独立应用，每个子目录单独配置 / 构建 |
| `extern_lib/` | **供 project_Demo 应用按需使用的第三方依赖**，如 FFmpeg；产物放到对应 build 目录 |
| `tests/` | 核心行为规格；修改机制时找对应断言 |
| `tools/` | 字库生成、资源校验、文档检查和构建矩阵 |

新增业务逻辑放应用层：Canvas 不负责图层融合，Grid 不解析电子表格公式，BarChart 不计算音频频谱，Image 不解码视频。需要多个应用共用的 GUI 行为再提炼到库；平台调用放 HAL 后端或应用桥接。核心移植范围是 `YMGUI/`，不需携带 `extern_lib/`。

## 3. 功能选型：优先复用现成控件

当前有 27 个具名控件，另有 base 容器。所有控件头文件均在 [WIDGET](../YMGUI/WIDGET)，以下按任务查找。

| 需求 | 控件 / 机制 | 使用提示 |
| --- | --- | --- |
| 简单文字、按钮、图标操作 | Label、Button | Label 适合短文本；Button 可显示图片、关闭背景、按住连发 |
| 表单与数值输入 | Checkbox、Switch、Slider | Slider 目前只验收水平实现；先设范围再设值 / 绑定 |
| 状态和仪表 | Bar、Arc、Spinner、Meter | Bar 显示进度；Spinner 依赖 Tick |
| 二维方向输入 | Joystick | 死区、圆形限位、松手回中 / 保持、可自定义皮肤 |
| 单行输入 | TextInput | UTF-8 光标 / 选区 / 剪贴板；创建时给容量 |
| 多行编辑 | EditView | 插入、选区、撤销、查找替换、折行和内边距 |
| 多行只读说明 / 日志 | TextView | 支持分行、折行和滚动；长文本别硬塞进 Label |
| 列表、下拉选项、滚轮选择 | List、Dropdown、Roller | Dropdown 用 top_layer；Roller 可吸附并平滑滚动 |
| 表格、树形导航、分页 | Table、Grid、TreeView、Tabview | Table 偏行展示；Grid 支持单元格选区；TreeView 支持懒加载 |
| 趋势 / 柱状图 | Chart、BarChart | 应用侧提供数据；BarChart 支持峰值顶标 |
| 图片 / 视频帧 | Image | `GY_IMG_NONE` 原尺寸；`GY_IMG_FIT` 保持比例；`GY_IMG_FILL` 拉伸铺满 |
| 画布和取色 | Canvas、ColorPicker | Canvas 自持像素、整数缩放 / 平移；图层与绘画工具在应用侧 |
| 消息确认 / 文件选择 | MsgBox、FileDialog | 使用异步结果回调；FileDialog 需注入文件系统实现；可用 SetTranslator 翻译界面文案 |
| 特殊视图 / 时间线 | base + `draw_cb` / `event_cb` | 复用图元和事件捕获，应用自己保存模型 |
| 多控件同步 | subject / bind | Label、Bar、Arc、Meter 单向；Slider、Switch、Checkbox、TextInput 双向 |
| 扩展模块 | PLUGIN | 静态注册适合固件；动态插件通过宿主函数表使用 UI |

暂未提供持续约束布局、通用 flex/grid 布局引擎、Dropdown / Table 索引绑定、FileDialog 内建复制粘贴或删除。不要把应用已经实现的能力误写成核心控件 API。

## 4. 可运行的最小桌面应用

在 `project_Demo/my_app/` 放下面两个文件；已有项目不必另建目录。模板只需 SDL2 开发包，使用本仓库公共构建入口。头文件采用裸头形式，如 `#include "YMGUI_Button.h"`，包含路径由公共 CMake 设置。

`CMakeLists.txt`：

```cmake
cmake_minimum_required(VERSION 3.16)
project(my_app C)
include(${CMAKE_CURRENT_LIST_DIR}/../ymgui_app.cmake)
ymgui_add_app(my_app main.c)
```

`main.c`（点击按钮更新绑定标签，参数 `30` 表示运行 30 帧后退出）：

```c
#include "YMGUI_Hal.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Button.h"
#include "YMGUI_Label.h"
#include "YMGUI_Bind.h"
#include "SDL_LCD.h"
#include <stdlib.h>

static GY_SUBJECT_INT(clicks, 0);

static void on_click(GYOBJ button)
{
    (void)button;
    YMGUI_State_SetInt(&clicks, YMGUI_State_GetInt(&clicks) + 1);
}

int main(int argc, char** argv)
{
    int result = 1;
    int frames = argc > 1 ? atoi(argv[1]) : -1;
    GYCTX ctx = NULL;
    GYdisp disp = {0};
    disp.hor_res = 320;
    disp.ver_res = 240;
    disp.buf_px_cnt = 320 * 32;
    disp.buf1 = GY_malloc1(disp.buf_px_cnt * sizeof(GYpx));
    if (!disp.buf1 || SDL_LCD_Init(&disp, 1) != 0)
        goto done;
    ctx = YMGUI_Creat_Ctx_Creat(&disp, 320, 240);
    if (!ctx)
        goto done;
    GYOBJ label = YMGUI_Creat_Label_Creat(ctx->root, 20, 30, 280, 30);
    GYOBJ button = YMGUI_Creat_Button_Creat(ctx->root, 100, 100, 120, 40);
    if (!label || !button)
        goto done;
    YMGUI_Label_Bind(label, &clicks);
    YMGUI_Button_SetText(button, "Click me");
    YMGUI_Button_SetClicked(button, on_click);
    YMGUI_Inject_SetCtx(ctx);
    while (frames != 0 && SDL_LCD_PumpEvents())
    {
        YMGUI_Refresh(ctx);
        SDL_LCD_Delay(16);
        if (frames > 0)
            --frames;
    }
    result = 0;
done:
    YMGUI_Inject_SetCtx(NULL);
    if (ctx)
        YMGUI_Free_CtxFree(ctx);
    SDL_LCD_Destroy();
    GY_free1(disp.buf1);
    return result;
}
```

从仓库根目录运行：

```bash
cmake -S project_Demo/my_app -B build/rgb565/project_Demo/my_app -DYMGUI_COLOR_DEPTH=16
cmake --build build/rgb565/project_Demo/my_app -j8
SDL_VIDEODRIVER=dummy ./build/rgb565/project_Demo/my_app/my_app 30
./build/rgb565/project_Demo/my_app/my_app
```

`dummy` 用于无头验证；最后一条打开真实窗口。模板以 RGB565 / RGB888 验证，内存紧张的目标可按实际屏幕和色深调整 buffer。完整交互示例见 [demo_button.c](../Demo/demo_button.c) 和 [demo_bind.c](../Demo/demo_bind.c)。

## 5. 常用技巧与生命周期

### 对象、状态和资源谁负责释放

- `YMGUI_Free_ObjFree(parent)` 会递归释放子对象；`YMGUI_Free_CtxFree(ctx)` 释放 root 和 top_layer。父对象销毁后，应用保存的子对象指针也失效，不可再手动释放一次。
- **现成控件的 `user_data` 通常存放内部私有结构**。不要把 Button / Label 等控件的 `user_data` 替换成应用上下文。可在自建 base 容器保存上下文，或在应用结构中保存控件句柄；`bind_data` 留给绑定系统。
- Label / Button 的 `SetText` 会拷贝文本，但有固定字节容量；Label 默认总缓冲 64 字节。UTF-8 一个汉字占多个字节，调用方应控制长度，避免在码点中间截断。容量和复制语义要逐控件查头文件 / 实现。
- Image / Button 图片源不复制像素，`GYimg` 描述符及其数据都要保持存活；不要传函数返回后失效的局部数组。
- `GY_malloc0/1` 中的 0 / 1 是内存区，不代表清零。分配后初始化字段、检查失败，并与同档 `GY_free0/1` 配对。`GYdisp` 用 `{0}` 初始化，避免可选回调或第二缓冲悬空。
- 后台线程只处理业务数据，用队列 / 同步机制交给 UI 线程更新对象和 subject；不要把控件接口当成线程安全接口。退出时先停止后台任务，再解绑应用观察者、销毁对象和资源。

### 用绑定减少同步代码

先创建控件、设置范围，再调用 `*_Bind`。状态更新用 `YMGUI_State_SetInt/Bool/Str`，直接修改结构字段不会自动通知。绑定时会立即向控件应用一次当前值；`YMGUI_State_AddObserver` 的应用回调只在后续变化时执行，初始化显示要自己安排。

双向绑定占用控件的 changed 回调；绑定后再调用 `SetChanged` 会破坏写回。业务监听改用 `YMGUI_State_AddObserver`，保留返回句柄并在释放上下文前 `YMGUI_State_RemoveObserver`，然后清空应用持有的句柄。控件自身的观察者在控件析构时自动摘链。

字符串 subject **只保存地址**。使用长期存活的缓冲，原地更新后调用 `YMGUI_State_Touch`。TextInput 双向绑定可能让 subject 指向输入框内部缓冲，输入框销毁前需处理仍要使用该字符串的订阅者 / 副本；subject 本身也应活得比绑定它的控件更久。见 [状态头文件](../YMGUI/STATE/YMGUI_State.h) 与 [绑定头文件](../YMGUI/STATE/YMGUI_Bind.h)。

### 更新界面、移动对象、自绘

- 用公开 setter 更新已有属性，它们通常自动标脏。直接更改 `area` / `scroll_x/y` / 自绘模型时，需要主动使受影响区域失效；移动对象应覆盖旧位置和新位置，不能只刷新新位置。
- 原地写入 Image 的像素后，调用 `YMGUI_Obj_Invalidate(image)`。单纯调用 `YMGUI_Refresh(ctx)` 不会使未标脏的图片重画。
- `area` 相对父对象，事件里的 `ctx->point_x/y` 是屏幕坐标。用 `YMGUI_Obj_GetAbsArea` 换算，尤其注意父容器滚动。
- `draw_cb` 的 `abs` 和图元坐标都是屏幕坐标；surface 只代表当前 band。优先用 `YMGUI_Draw_*`，不要按全屏地址写 `surface->buf`。自绘回调可能一帧执行多次，避免在里面推进业务状态、读文件或解码视频。
- `YMGUI_Layout_Stack/Align` 只在调用时计算位置；增加子对象或改变尺寸后需重新调用。容器的 `GY_STATE_ClipChildren` 控制子裁剪，不会自动产生滚动条。
- 包装已有控件 `event_cb` 时，保存并按需要调用原回调；直接替换会丢掉原控件的点击、编辑或拖动逻辑。

### 输入、模态和时钟

先 `YMGUI_Inject_SetCtx(ctx)` 再注入输入。SDL 事件泵会注入 Tick，裸机循环调用 `YMGUI_Inject_Tick(elapsed_ms)`，传实际经过的毫秒数；重复注入会加速动画。HAL 注入上下文、按键过滤器和全局字体回退等接口有全局状态，多窗口 / 多上下文不能直接假定隔离。

普通拖动捕获起点对象，移出后仍收到 Pressing；取消通过 `YMGUI_Inject_PointerCancel` 清理，走 ReleasedOff 而不是 Clicked。右键 / 触摸长按另有 ContextRequested / Dragging / Released / Cancelled 链。读 [事件头文件](../YMGUI/GUI/YMGUI_Event.h) 和 [HAL](../YMGUI/HAL/YMGUI_Hal.h)，不要自行伪造一串左键点击。

TextInput / EditView 有聚焦和编辑两种状态；程序设置焦点不一定等于进入编辑态。MsgBox / FileDialog 用回调续接动作，不要阻塞主循环等待用户选择，否则界面无法继续处理输入。

### 中文、图片与色深

中文源码用 UTF-8；内建 CJK 只覆盖精简字集，不能假定所有汉字都有字模。完整 GB2312 和扩展符号使用 [demo_font_gb2312.c](../Demo/demo_font_gb2312.c) 的运行期字体回退方式，读取 `tools/gb2312_glyphs.bin`。字体对象、读取回调和数据源要保持存活，退出时解除回退再关闭数据源。新增字形需同步生成字模和码点索引，并运行资源校验。

`GYcolor` 是 API 的 ARGB 颜色，`GYpx` 是当前色深的存储格式。用 `GY_ARGB` 造颜色，需写原生像素时用 `GY_ColorToPx`；RGB888 的 `GYpx` 是三字节结构，不能统一按 `uint16_t` 或整数比较处理。编译库、应用和插件必须使用相同色深。

## 6. 构建、配置和验证入口

默认输出布局和清理范围见 [构建目录说明](BUILD_LAYOUT.md)。所有命令默认在仓库根执行。根工程编译核心、Demo 和核心测试，**不包含 project_Demo 的独立应用**。

```bash
cmake -S . -B build/rgb565/Demo -DYMGUI_COLOR_DEPTH=16
cmake --build build/rgb565/Demo -j8
ctest --test-dir build/rgb565/Demo --output-on-failure
```

针对机制可选测，例如 `ctest --test-dir build/rgb565/Demo -R 'test_(event|focus|bind)$' --output-on-failure`；需先构建对应测试可执行文件。改应用则重新构建该应用的独立目录，并运行它自己的测试 / 自测与启动检查。

| 入口 | 范围 |
| --- | --- |
| `tools/check_repo.sh` | 文档链接、资源一致性、计数和 diff 空白检查；不编译 |
| `tools/test.sh --depth 16` | 根工程单色深构建与测试，默认 `build/rgb565/Demo` |
| `tools/test_matrix.sh` | 根工程 RGB565 + RGB888；`--all-depths` 加入 1 / 8 bpp |
| `./sdk/build.sh` | 原生 Linux 二进制 SDK，双色深核心 / 可选 SDL 库及脱离源码的接入验证 |
| `./build_all.sh` | 按色深分组构建 Demo 和完整应用；默认 RGB565，`--depth 24` 跳过 video_stidio；`-t` 跑根工程 CTest |
| `./capture_shots.sh` | 从统一构建目录批量截图；`--depth` 选择色深，`--output` 指定验证输出目录 |

像素、字体、渲染等通用变化应覆盖 RGB565 / RGB888；修改裁减路径再验证相关宏或其他色深。每种色深用独立 build 目录，勿覆盖另一种 ABI 的产物。功能开关定义在 [PubDefine](../YMGUI/CONFIG/YMGUI_PubDefine.h)，像素类型在 [PubType](../YMGUI/CONFIG/YMGUI_PubType.h)。`-DYMGUI_COLOR_DEPTH=24` 是已接入的 CMake 变量；其他头文件宏并非都能直接作为 CMake 参数，先查 CMake 是否会传给编译器。裁减控件时应用也要避开被裁掉的调用。

外部工程需要预编译库时，先看 [SDK 说明](../sdk/README.md) 和 [接入手册](../sdk/MANUAL.md)。`YMGUI_DIR` 指向包内 `cmake/`，链接 `YMGUI::ymgui` 或 `YMGUI::sdl`，色深由导入目标统一传递；不要将仓库 `ymgui_app.cmake` 和 SDK 混用。SDK 固定默认 ABI，裁减功能、改坐标位宽或移植平台必须重建库。`--without-sdl` 生成纯核心包；`--ymgre-dir` 可验收 YMGRE 联用。

应用特例：

- `chinese_ime` 的字词库有独立 EXTERNAL / EMBEDDED / OFF 等选项和自测，具体以其 [README](../project_Demo/chinese_ime/README.md) 与 CMake 为准。
- `music_studio` 是中文音乐制作应用，仅桌面 Linux / RGB565 / RGB888；模型、音频与界面分层，导入需 FFmpeg CLI，内置播放和 WAV 导出仅用 SDL / libm。独立构建目录中运行 2 项 CTest 或 `--selftest`，详见其 [架构与 AI 说明](../project_Demo/music_studio/ARCHITECTURE.md)。
- 修改音乐工坊钢琴/轨道交互前，先读 [编辑操作指南](../project_Demo/music_studio/docs/编辑操作.md)：Ctrl＋滚轮只缩放显示，步长/音长才影响时值；保持缩放后的绘制、命中、放置、拉长、框选和鼓机点击使用同一坐标映射。视口与选择不写工程历史，相关输入注入回归在 `project_Demo/music_studio/tests/ui_test.c`。
- 音乐工坊的可编辑曲库由 [离线转换脚本](../project_Demo/music_studio/tools/build_library.py) 和 `library/catalog.json` 维护，仅需 Python 3 标准库；运行 `python3 project_Demo/music_studio/tests/test_library.py` 检查转换回归。新增曲目、源文件校验、配置字段及能力边界先读 [曲库说明](../project_Demo/music_studio/library/README.md)，不要把它当作应用内通用 MIDI 导入器。
- `video_stidio` 名称按现有目录拼写保留；仅桌面 Linux、C11、RGB565，不能套用所有应用的双色深或 Android 构建。默认源码依赖为 `extern_lib/FFmpeg`，代理 / 编码另用 FFmpeg CLI。其 [README](../project_Demo/video_stidio/README.md) 提供 5 项独立 CTest，有限帧启动使用 `--frames 120`。
- 共享 CMake 支持 Windows / Android 不代表每个应用均完成移植。先读应用依赖和 [跨平台移植](CROSS_PLATFORM_PORTING.md)，特别是 POSIX 文件系统、子进程、资源路径和插件动态加载部分。

## 7. 定位问题的最短路径

| 现象 | 先检查 |
| --- | --- |
| 修改后看起来没变化 | 构建的是否为当前应用及色深目录；对象是否标脏；是否运行了旧二进制 |
| 窗口初始化失败 | `SDL_LCD_Init` 返回值、显示会话 / SDL 驱动、SDL2 依赖；无头用 dummy，不据此认定真实窗口成功 |
| 控件不响应点击 / 输入 | 注入上下文、命中层级、Hidden / 模态层、焦点 / 编辑态，是否覆盖了原事件回调 |
| 更新数值但绑定没动 | 是否调用 Set / Touch；是否覆盖 changed 回调；subject 是否存活 |
| 文字被截断或中文空白 | 控件字节容量 / 宽度、UTF-8 边界、字库覆盖、blob 路径和全局 fallback |
| 图片花屏、颜色错误 | `GYimg` 与像素生命周期、stride、`GYpx` 色深一致性、原地更新是否标脏 |
| 拖动坐标偏移或残影 | 屏幕 / 父坐标转换、父滚动、捕获状态、旧区域是否失效 |
| DMA 刷新卡住 | `buf2`、flush_busy、DMA 完成后的 `YMGUI_Disp_FlushReady`、wait_cb；参照 `test_async` |
| 关闭 / 卸载后崩溃 | 后台任务未停、observer 未摘链、图片 / 字符串悬空、父子重复释放、动态库回调仍被对象引用 |
| 断言后仍继续执行 | `gy_assert` 是项目日志断言，不能代替显式错误返回 |

先复现最小路径，再改所属层。搜索示例：`rg -n 'YMGUI_Image_SetScaleMode' YMGUI Demo project_Demo`；不要默认扫描整个 `extern_lib/FFmpeg`。

## 8. 值得直接读的示例

| 学什么 | 入口 |
| --- | --- |
| band 刷新与显示闭环 | [demo_flush_band.c](../Demo/demo_flush_band.c)、[test_band.c](../tests/test_band.c)、[test_async.c](../tests/test_async.c) |
| 表单 / 状态联动 | [demo_form.c](../Demo/demo_form.c)、[demo_bind.c](../Demo/demo_bind.c)、[test_bind.c](../tests/test_bind.c) |
| 自绘 / 布局 | [demo_draw.c](../Demo/demo_draw.c)、[demo_layout.c](../Demo/demo_layout.c) |
| 多行文本 / 编辑器 | [demo_textview.c](../Demo/demo_textview.c)、[txt_edit](../project_Demo/txt_edit/README.md) |
| 右键 / 触摸长按 / 拖动取消 | [context_gesture](../project_Demo/context_gesture/README.md)、[test_event.c](../tests/test_event.c) |
| 文件系统回调与对话框 | [FileDialog 头文件](../YMGUI/WIDGET/YMGUI_FileDialog.h)、[test_filedialog.c](../tests/test_filedialog.c) |
| 树 / 电子表格 / 图层画布 | [files_manager](../project_Demo/files_manager/README.md)、[excel_edit](../project_Demo/excel_edit/README.md)、[image_edit](../project_Demo/image_edit/README.md) |
| 后台媒体任务与 UI 分离 | [video_stidio 架构](../project_Demo/video_stidio/ARCHITECTURE.md) |
| 实体 / 虚拟键盘与 CJK | [chinese_ime](../project_Demo/chinese_ime/README.md)、[test_sdl_context.c](../tests/test_sdl_context.c) |
| 插件宿主与生命周期 | [plugin_host](../project_Demo/plugin_host/README.md)、[test_plugin.c](../tests/test_plugin.c) |

## 9. 完成修改后维护哪些文档

遵守 [AGENTS.md](../AGENTS.md)：功能、边界、构建或计数变化同步到当前状态；每轮实质开发续写 DEVLOG V2；公开 API 变化更新 API，架构 / 移植变化更新专题文档。本文维护“选型与用法”，无需复制全部函数签名或每轮测试计数；README 保持简短入口。增加应用、测试或控件时还需核对 `tools/check_repo.sh` 中的计数约束。
