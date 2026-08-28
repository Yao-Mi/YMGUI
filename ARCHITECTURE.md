# YMGUI 架构文档

本文档说明 YMGUI 的分层、核心类型、渲染管线，以及各机制的设计取舍。面向要理解或扩展这个库的人。

## 一、整体分层（自底向上，严格单向依赖）

```
CONFIG   ← 平台适配 + 编译期配置(COLOR_DEPTH / coord 宽度 / buffer 尺寸 / 数学宏 / 裁剪开关)
   ↑
HAL      ← 显示驱动接口 + 输入注入接口(纯头,最窄腰部 flush(area,buf))
   ↑
COMMON   ← 平台无关纯算法(几何 rect 运算、定点三角),不碰 GYsurface
   ↑
OPOBJ    ← 对象生命周期(Creat/Free,统一内存出入口,★树级联)
   ↑
CORE     ← 软件光栅化层(碰 GYsurface/framebuffer 的全在这:fill/line/glyph/img/arc)
   ↑
GUI      ← ★GUI 机器(失效·脏矩形·事件分发·焦点)—— 保留模式发动机
   ↑
WIDGET   ← button/label/checkbox/slider/list...
   ↑
SDL_LCD  ← HAL 的具体实现(Linux 假 LCD → 换成真实 LCD)
   ↑
DEBUG    ← 横切层(assert/log),被所有层调用
```

上面 CONFIG/COMMON/OPOBJ/CORE/GUI/WIDGET/HAL/DEBUG/STATE 九层都在库根 **`YMGUI/`** 下（对齐 YMCV 的 `OpenSrc-YMCV/YMCV/`），自包含、移植时整个拷走；`SDL_LCD/`、`Demo/` 是 PC 演示外壳，留在顶层不带走。

**归位判据**（决定新文件放哪）：
- 是否直接读写 `GYsurface`/framebuffer 像素 → 碰像素进 `YMGUI/CORE/`；纯几何/数据结构进 `YMGUI/COMMON/`。
- 是否是 GUI 交互机制（失效/事件/焦点/滚动）→ 进 `YMGUI/GUI/`。
- 控件 → `YMGUI/WIDGET/`；平台实现 → `SDL_LCD/` 或用户自建 LCD port。

## 二、最窄腰部：显示 HAL

整个库对硬件的要求收敛到一个回调。这是"桌面开发/裸机部署零分叉"的关键。

```c
typedef struct GYdisp
{
    GYcoord hor_res, ver_res;   // 屏幕尺寸
    GYpx*   buf1;               // draw buffer，可远小于整屏
    GYpx*   buf2;               // 可选第二块(异步双缓冲，当前未启用)
    uint32  buf_px_cnt;         // 每块能放多少像素 → 决定 band 高度
    void (*flush_cb)(struct GYdisp* d, const GYrect* area, const GYpx* buf);
    void*   user_data;          // SPI 句柄 / SDL_Texture 等
}GYdisp;
```

- **硬件只需实现 `flush_cb`**：把 buf 里 area 大小的连续像素推到面板。SDL 实现用 `SDL_UpdateTexture` 上传 band，并通过可选的 `YMGUI_Disp_SetFrameDoneCb` 在整帧完成后 `RenderPresent`；真机是 SPI/并口 DMA，通常无需整帧回调。
- **画点不作必选**：它只是 flush 一个 1×1 area 的退化情况，绝大多数面板支持窗口 blit。
- **输入走注入**（拉取式，裸机无消息泵）：`YMGUI_Inject_Pointer(x,y,pressed)` / `YMGUI_Inject_Key(key,pressed)`。port 把自己的事件源翻译成注入调用，库核心对"事件哪来的"一无所知。

## 三、核心类型

### 3.1 三层颜色分离（重要，别混用）

framebuffer 存 RGB888/RGB565/灰度/1bpp（无 alpha；RGB888 每像素 3 字节），但抗锯齿/半透明仍需 alpha。所以三个概念严格分开：

| 概念 | 类型 | 用途 | 存在于 |
|------|------|------|--------|
| API 颜色 | `GYcolor`(32位 ARGB) | 用户描述颜色 `0xFF3080C0` | 公开 API |
| 表面像素 | `GYpx`(编译期=RGB888/RGB565/灰度/1bpp) | framebuffer 实际存储 | CORE + HAL |
| 覆盖度 | `GYopa`(8位) | 混合那一瞬间 | CORE 混合内部 |

编译期由 `YMGUI_COLOR_DEPTH` 宏切换 `GYpx`。混合流程：读目标 `GYpx` → 升 8bit → 用 `GYcolor`+`GYopa` 混合 → 打包回 `GYpx`。**副作用**：1bpp 无法抗锯齿（只能阈值化），灰度/565 可以。

### 3.2 数值类型（无 FPU 友好）

| 类型 | 定义 | 用途 |
|------|------|------|
| `GYcoord` | int16(默认)/int32 | 几何/布局，整数像素是工作单位 |
| `GYvalue` | int32(16.16 定点) | 子像素/动画/字体度量 |

主几何保持整数，float 不出现在必经路径。数学走 `CONFIG/` 封装宏(`YMGUI_Sqrt/Sin`)，移植改宏右边。定点三角另有 `COMMON/MATH` 的 Q15 查表 `GY_Sin/Cos`(无 libm)。

### 3.3 Surface（光栅化目标）

surface 不是"整屏"，而是"当前 band 在屏幕坐标系里的一个窗口"：

```c
typedef struct {
    GYpx*   buf;       // 指向 draw buffer
    GYrect  buf_area;  // 这块 buffer 映射到屏幕的矩形
    GYrect  clip;      // 当前裁剪区(屏幕坐标)
    GYcoord stride;    // 一行多少像素
}GYsurface;
```

所有绘制函数收**屏幕坐标**，内部裁剪到 `clip`、再平移到 `buf_area` 原点写入。图元完全不知道有没有 OS、是 SDL 还是真 LCD —— **这是可移植性的锚点**。

## 四、渲染管线：一帧怎么走

保留模式 + 分块刷新的核心。空闲时几乎不耗（无脏区直接返回）。

```
主循环:
  1. SDL_LCD_PumpEvents() → 翻译事件 → YMGUI_Inject_*
       → 命中测试 → 事件分发 → 控件改状态 → 标记失效(脏矩形)
  2. YMGUI_Refresh(ctx):
       若无脏区 → 直接返回(省电关键)
       for 每块脏矩形:
         横向按 tile_w 切列, 纵向按 band_h 切行(块 ≤ buf 容量)
         for 每块:
           surface.buf_area = 这块的屏幕矩形; clip = buf_area
           drawObjRec(root): 递归重绘与本块相交的对象
           flush_cb(块, buf)   ← 唯一碰硬件处
```

**关键点**：
- draw buffer 只够 40 行？照样跑，flush 次数多而已。够整屏？一次 flush。同一套代码覆盖 MCU→桌面全谱系。
- 脏矩形只重绘变化区域 → LCD 的 blit（最贵操作）降到最低。
- 块大小永不超过 `buf_px_cnt`（横向也切），保证不越界。

**异步双缓冲（`buf2 != NULL` 才激活；单缓冲零改动）**：DMA 传一块的时间里，CPU 本该干等——给两块 buffer 就能重叠掉。调度状态机在 `YMGUI_Refresh`/`refreshOneRect`，**不在 HAL**（HAL 不知道"band"是什么，只提供 `buf2`/`flush_busy`/`FlushReady` 三个原语，GUI 拿它们轮转）。

```
游标 cur 在 buf1/buf2 间 ping-pong;帧入口先排空上帧遗留 DMA
for 每条 band:
  drawObjRec → cur          渲染进当前块
  waitFlushIdle()           等上一块 DMA 传完(至多 1 个在途)
  flush_busy=1; flush_cb()  发起本块传输(异步 port 立即返回)
  cur 切到另一块            ← 下条 band 渲染与本块 DMA 重叠
帧尾再 waitFlushIdle()      返回后所有 buffer 空闲,下帧可安全复用
```

- 不变式：任一时刻至多 1 个在途传输；绝不写正在传的 buffer。
- `waitFlushIdle` 有 `wait_cb` 就调（裸机可填 `__WFI` 省电），否则忙等自旋。同步 port 的 flush_cb 返回前已调 `FlushReady`，故此等待立即通过——单缓冲路径完全不碰这套逻辑。
- 桌面无真 DMA，`test_async` 用"flush_cb 只标在途、wait_cb 里调 FlushReady 模拟 DMA 完成中断"在单线程验证时序正确。

**平台相关回调清单**（移植时要改函数体的就这几处，共性在此收口，不硬塞 BSP 层）：

| 回调 | 层/位置 | 何时改 |
|------|---------|--------|
| `GY_malloc0/1` `GY_free` | `CONFIG/YMGUI_Mem.c` | 必改:换平台分配器(小/快 vs 大/慢分档) |
| `GYdisp.flush_cb` | 用户 LCD port | 必改:SPI/并口把像素推上屏 |
| `GYdisp.FlushReady` 调用点 | LCD port(同步末尾/异步中断) | 异步双缓冲才用 |
| `GYdisp.wait_cb` | LCD port(可选) | 想省电填 `__WFI`,否则 NULL 忙等 |
| `Inject_Pointer/Key` | 用户输入层 | 触摸/按键读数翻译成注入调用 |
| `GYfont.glyph_read` | app 自造 GYfont(见字体节) | 方案3 外部 flash 才用 |

## 五、GUI 机器（YMCV 无对应，全新）

这是"GUI 之所以是 GUI"的部分，YMCV(无状态变换库)帮不上忙。

### 5.1 对象树 + 树级联释放

对象 = `GYobj` + 3 个回调(draw_cb/event_cb/free_cb)。树用 `child_head + sibling` 链，坐标相对父对象。

**与 YMCV 的关键区别**：YMCV 对象扁平（谁申请谁释放）；YMGUI 是树，`YMGUI_Free_ObjFree(container)` 递归级联释放所有子节点。释放顺序：先递归释放子树 → 标脏占用区 → 清 ctx 里对本对象的引用(pressed/focus) → 从父链摘除 → 调 free_cb 清 user_data → 释放自己。

### 5.2 失效/脏矩形（多矩形列表）

`GYctx` 持 `inv_areas[GY_INV_MAX=16]` 非重叠矩形列表。`InvalidateArea` 用**连通分量合并**维护"互不重叠"不变式：新区与任何已有块接触就反复并成一块，槽满则塌缩为单包围盒。

为什么多矩形而非单包围盒：两块相距很远的小改动，单包围盒会合并成一个巨大矩形把中间没变的区域全重绘。实测两个远端 20×20 脏区，多矩形刷 ~800px，单包围盒要刷 76800px（96× 差距）。

### 5.3 事件分发 + 焦点

- 命中测试：后序递归（靠后的兄弟层级更高），返回最深命中对象。
- 指针语义：按下捕获 `pressed_obj`；按住移动派发 `Pressing` 给已捕获对象（即使移出其范围，拖动仍归它）；抬起时按下抬起同一对象则 `Clicked`，否则 `ReleasedOff`。
- 焦点：命中 `GY_STATE_Focusable` 对象则获焦（旧焦点收 FocusLost、新焦点收 FocusGot），命中非聚焦对象清焦点。键盘 `Inject_Key` → 派发 `GY_EVENT_Key` 给焦点对象，键值在 `ctx->last_key`。
- 坐标传递：控件事件回调收不到坐标，改从 `ctx->point_x/y` 读（LVGL 同款做法）——Slider 拖动、List 滚动都靠这个。

### 5.4 滚动 + 子裁剪机制

- `GYobj` 有 `scroll_x/y` + `GY_STATE_ClipChildren` 状态位。
- `GetAbsArea` 累加父原点时**减去父的 scroll**（父滚动作用于所有子孙）。
- `drawObjRec` 遇 `ClipChildren` 父，递归子对象前把 `surface.clip ∩ 父abs`，递归后恢复 → 超出视口的子被裁。
- List 容器就是开 ClipChildren + 拖动改 scroll_y（钳到 `[0, content_h-视口高]`）。这套机制通用，Tabview/Dropdown 都可复用。

## 六、图元层（CORE）

全部收屏幕坐标，共享 `CORE/YMGUI_DrawPx.h` 的 `inline GY_PutPx`（裁剪+band偏移+buf_area 边界防御一处实现）。

| 图元 | 算法 | 说明 |
|------|------|------|
| DrawFill | 逐行填充 | 不透明直写 / 半透明读-混合-写回 |
| DrawText | 位图字体取位 | UTF-8 解码 + 回退链;1bpp 硬边 / 4bpp 灰度 AA;字模可放外部 flash |
| DrawLine | Bresenham | 水平/垂直特判走快速路径 |
| DrawImg | blit | native GYpx 数据，可选 colorkey 透明 |
| DrawArc | 中点画圆 + 定点三角 | Circle/CircleFill/Arc/ArcThick(粗弧环) |

**粗弧环 ArcThick** 用径向填充（角度按外圆弧长自适应细分 + 每角度沿半径逐像素连填），周向+径向双向无缝，解决了多层独立画圆弧留黑洞的问题。

**字体**：`Demo/gen_font.py` 把 TTF 栅格成定宽点阵，生成 `CORE/YMGUI_FontData*.c`。选位图而非 stb_truetype 是裸机决策（无 FPU/无依赖/纯数据）。默认 ASCII 8×16 连续 32..126；CJK 16×16 4bpp 用稀疏排序 `codepoints[]`(二分查找)。

**中文/CJK 三层设计**（`YMGUI_FONT_CJK` 可整体裁掉）：
- **回退链**：`GYfont` 有 `fallback` 字段。默认 ASCII 字体遇 CJK 码点自动下探 `YMGUI_Font_CJK`；`cell_h` 一致时基线自动对齐。所有控件仍用 `&YMGUI_Font_Default` + `const char*`，零改动出中文。
- **运行期全局兜底钩子**：`YMGUI_Font_SetFallback(font)`（一个 `.bss` 指针，默认 NULL）——任一字体的 const `fallback` 链走空后再下探它。用途：把"运行期才知道的"字库(如放外部 flash 的 GB2312 全集)一句话灌进所有控件回退链。
- **外部 flash 缝**：`GYfont.glyph_read(font,off,len,buf)` 回调。NULL=直接用 bitmap 指针(内部/映射 flash)；置了=先拷字节进 RAM 再 blit(非映射 SPI flash)。库永不碰通讯协议，移植真机只改回调函数体。一个 16×16 4bpp 字模 = 128 字节，第 i 字在 i×128 偏移。
- **三轴正交**：CJK 开关(编译期宏) / 字集范围(生成期脚本参数,非宏) / 字模存放(运行期回调)——各归其位，见 `API.md`。

## 七、三条横切主线（继承 YMCV）

1. **内存双档制**：`GY_malloc0`(小/快，对象头/样式/LUT → CCM/TCM) vs `GY_malloc1`(大/慢，framebuffer/图 → SDRAM)。移植只改 `CONFIG/YMGUI_Mem.c` 函数体。
2. **生命周期成对**：`YMGUI_Creat_* / YMGUI_Free_*` 镜像，创建骨架固定(malloc→校验→填字段→返回)。
3. **入口防御三连**：`gy_assert` + `gy_log_explain(cond, 错误码, "中文提示")`，错误码位掩码 `GYEVNLOG`，发布可整层关闭。注意 `gy_assert` 只打印不中止，故 malloc 失败后需显式 `return NULL`。

## 八、性能哲学

面向嵌入式（无 FPU、内存紧）：查表 LUT（渐变/三角/字形）、整数/移位换浮点、位打包（1bpp）、脏矩形省 blit。规整逐像素核在 PC `-O3` 下还能吃自动 SIMD，与 LUT 路线是两条路（详见 `代码风格.md` §12 继承的讨论）。

## 九、可裁剪性

`CONFIG/` 里 `YMGUI_XXX_USE` 开关，功能代码 `#if ... #endif` 包裹，按 Flash/RAM 预算砍模块。`GY_INV_MAX`(脏矩形数)、`GYGUI_COLOR_DEPTH`、`GYcoord` 宽度都是编译期可调的裸机旋钮。`YMGUI_ANTIALIAS`(抗锯齿，单色屏强制关) / `YMGUI_FONT_CJK`(中文，关掉回退链断开、CJK 字模数据整段 `#if` 编译成空、无悬空符号，回到纯 ASCII 足迹) 是两个大件裁剪开关。
