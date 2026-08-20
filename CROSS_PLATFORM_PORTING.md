# YMGUI 跨平台移植指南

YMGUI 核心只关心显示区域、像素、对象树、失效区和抽象输入事件。平台差异应留在 `SDL_LCD/` 或应用自己的桥接层，不要在 Button、List、Table 等控件中判断 Windows、Android 或 SDL 事件类型。

```text
平台 API / SDL 事件
        ↓
SDL_LCD 输入、窗口、刷新适配
        ↓
YMGUI_Inject_* / GYdisp.flush_cb
        ↓
YMGUI 核心与控件
```

## 1. 显示适配

`SDL_LCD` 把 SDL 当作一块“假 LCD”：

- YMGUI 继续使用软件光栅化和 band 分块刷新；
- `sdlFlushCb` 把 `area + buf` 上传到 SDL texture；
- 真机移植时只需用 SPI、并口、DMA 或平台 framebuffer 实现同一 `flush_cb`；
- 屏幕尺寸、texture 尺寸与 YMGUI 坐标系必须一致。

桌面 renderer 优先使用硬件加速和垂直同步；创建失败时回退到 software renderer，以兼容远程桌面、虚拟环境和没有可用加速驱动的平台。初始化 Window、Renderer 或 Texture 失败时，调用方应检查 `SDL_LCD_Init` 的返回值并停止进入 GUI 主循环。

## 2. 鼠标与触摸

桌面鼠标的 down、up、motion 映射为 `YMGUI_Inject_Pointer`。窗口使用放大倍数时，鼠标窗口坐标需除以 `scale` 还原为 YMGUI 屏幕坐标。

SDL finger 事件的坐标是 `[0, 1]` 归一化浮点数。`SDL_LCD` 将其转换到 `0..width-1`、`0..height-1` 并钳制边界，处理：

- `SDL_FINGERDOWN`
- `SDL_FINGERMOTION`
- `SDL_FINGERUP`

YMGUI 当前采用单 captured-pointer 模型，因此 SDL 适配层只跟踪第一个活动手指。多指缩放、旋转等手势应通过独立的高层 API 实现，不应改变普通控件的点击和拖动语义。

SDL 在移动平台上可能为一次触摸同时生成 finger 和模拟 mouse 两套事件。鼠标路径必须过滤 `SDL_TOUCH_MOUSEID`，否则一次触摸会被重复注入。应用进入后台或终止时还应释放活动指针，避免恢复后控件一直处于 pressed 状态。

鼠标右键和触摸长按统一为上下文操作，控件不区分平台来源。右键短点击只派一次 `GY_EVENT_ContextRequested`；右键相对按下点移动超过 4 个逻辑像素后开始捕获式拖动，事件序列为 `ContextRequested → ContextDragging* → ContextReleased/ContextCancelled`，不会再补发短点击。右键按下期间 SDL 捕获鼠标，拖出窗口后仍能收到抬起。

SDL 长按规则为保持 600ms、相对起点移动不超过 10px、每次触摸最多触发一次。识别成功时先调用 `YMGUI_Inject_PointerCancel()` 结束普通按压，再以触摸起点 `ContextBegin`；后续 motion/up 走 ContextMove/End，因此既可长按后继续拖动，也不会补发普通 `Clicked`。上下文拖动始终派给起点捕获对象，结束或取消前先清捕获。

静止手指不会持续产生 SDL 事件，所以应用必须周期调用 `SDL_LCD_PumpEvents()`；消息泵在队列抽干后用 `SDL_GetTicks()` 检查超时。32 位 tick 要用无符号差值 `(now - start) >= timeout` 判断，以正确跨越回绕。后台、终止和退出同样使用 PointerCancel，不能伪造一个 `(0,0)` 抬起。裸机 port 可在自己的周期任务中用相同规则识别长按。

## 3. 键盘与文本输入

可打印文本和控制键应分开处理：

- `SDL_TEXTINPUT` 注入文本；
- `SDL_KEYDOWN` 映射退格、删除、方向、Home/End、Tab 和 Ctrl 组合键。

不要同时从 `KEYDOWN` 和 `TEXTINPUT` 注入普通字符，否则中文输入法、组合输入和 Android 软键盘可能产生重复字符。

## 4. 资源路径

发布程序不要依赖启动时的当前工作目录。推荐按平台确定资源根目录：

| 平台 | 外部资源目录 |
|---|---|
| Linux | 可执行文件所在目录或安装数据目录 |
| Windows | EXE 所在目录 |
| Android | `Context.getFilesDir()` 等应用私有目录 |
| 裸机 | flash 地址、文件系统挂载点或资源读取回调 |

字库、图片和配置应由应用层解析绝对路径，再传给 YMGUI；Android assets 可由 Java/Kotlin 层复制到私有目录，或由专用桥接层读取。

## 5. CMake 构建

`project_Demo/ymgui_app.cmake` 统一负责构建 `ymgui`、`sdl_lcd` 和应用目标：

- Linux：通过 `pkg-config` 查找 SDL2；
- Windows：使用 SDL2 CMake package 的 `SDL2::SDL2`，存在时链接 `SDL2::SDL2main`；
- Android：使用 SDL2 CMake target，应用目标构建为 shared library；
- `YMGUI_SDL_STATIC=ON` 时优先选择可用的 `SDL2::SDL2-static`；
- Windows 不链接 Unix 的 `libm`。

Linux 示例：

```bash
cmake -S project_Demo/alarm_clock -B build/project_Demo/alarm_clock
cmake --build build/project_Demo/alarm_clock -j
```

Windows/Android 交叉构建时应通过 `SDL2_DIR` 指向 SDL2 导出的 CMake package，并提供对应工具链文件。

## 6. 验证清单

每个平台至少验证：

1. 从空构建目录完成 configure 和 build；
2. Window、Renderer、Texture 任一初始化失败时返回错误且资源被释放；
3. 鼠标点击、拖动、双击和键盘输入；
4. 真机触摸按下、拖动、释放，确认没有 finger/mouse 双重事件；
5. 进入后台再恢复后没有残留 pressed 状态；
6. 字库、图片等外部资源从非工作目录启动时仍能找到；
7. 退出后窗口、texture、renderer 和截图缓冲均释放。

Linux 可以使用 `SDL_VIDEODRIVER=dummy` 做无头启动和截图自检，但无头模式不能替代真实窗口、Windows 和 Android 真机输入测试。

## 7. 常见错误

- 在控件中直接处理平台事件，导致桌面和移动端行为分叉；
- 忘记过滤 `SDL_TOUCH_MOUSEID`，一次触摸触发两次点击；
- 只处理 `FINGERUP`，应用切后台后留下永久按下状态；
- 初始化中途失败直接返回，泄漏已创建的 SDL 资源；
- 只检查 CMake 配置成功，没有从零编译、运行并检查最终产物；
- 把应用专属的静态链接开关或资源路径写入通用 YMGUI 构建层。
