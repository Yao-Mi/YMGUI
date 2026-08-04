# YMGUI

面向**嵌入式/裸机优先**的跨平台 GUI 库，C99 + GNU 扩展。软件光栅化 + 保留模式 + 分块刷新，桌面(Linux/SDL)开发、裸机(MCU + LCD)部署，两者共用同一套渲染代码，只换一个显示驱动回调。

## 状态

25 控件 · 5 类图元 · 30 单测全过 · 23 个交互 demo · ~8900 行库代码 · clean build 0 错 0 警。
（Button/Label/Checkbox/Switch/Slider/Bar/Image/Arc/Spinner/Meter/TextInput/TextView/EditView/List/Chart/Dropdown/Table/Tabview/TreeView/Grid/Canvas/ColorPicker/Roller/BarChart + base 容器）

抗锯齿(4bpp 灰度字体 + Wu 斜线 + 距离场圆弧，`YMGUI_ANTIALIAS` 可裁)、中文/CJK(UTF-8 回退链 + 稀疏字模 + 外部 flash 回调，`YMGUI_FONT_CJK` 可裁)、状态/数据绑定地基均已落地。

## 构建 & 运行

```bash
sudo apt-get install libsdl2-dev      # 仅桌面开发/demo 需要，库本体不依赖 SDL
cmake -B build -S . && cmake --build build
cd build && ctest --output-on-failure # 跑全部单测
./build/demo_dashboard                # 看效果(需显示器)
./build/demo_font_gb2312              # 中文字体(GB2312 全集走外部 flash 回调)
```

## 文档导航（按需读）

| 想做什么 | 读哪份 |
|---------|-------|
| **第一次接触，先搞懂全貌** | `PROJECT.md`（是什么/为什么/进度/目录/构建） |
| **理解架构、扩展库** | `ARCHITECTURE.md`（分层/核心类型/渲染管线/各机制设计） |
| **写代码，查控件怎么用** | `API.md`（控件速查 + 最小程序骨架 + demo 索引） |
| **写代码，守规范** | `代码风格.md`（命名/格式/目录归位/性能惯用手法） |
| **了解怎么一路走来、踩过啥坑** | `DEVLOG.md`（逐轮进程 + 问题记录） |

## 一分钟理解设计

- **最窄腰部**：整个库对硬件只要一个回调 `flush(area, buf)` 把像素推到面板。移植 = 把 `YMGUI/` 整个拷走 + 改 `YMGUI/CONFIG/` + 照 `SDL_LCD/` 写真实 LCD 的 flush。
- **分块刷新**：draw buffer 可远小于整屏，逐块渲染逐块 flush，同一套代码覆盖几十 KB 的 MCU 到桌面。
- **保留模式 + 脏矩形**：控件树持久存在，只重绘变化区域，空闲不耗 —— LCD 的 blit 是最贵操作，这样降到最低。
- **无 FPU 友好**：几何用 int16，三角/渐变查表(Q15 定点)，不走 float 必经路径。
- **中文三轴可裁**：CJK 开关是编译期宏(轴1)，字集范围(精简/GB2312)是字模生成期参数(轴2)，字模存放(内部/外部 flash)是运行期 `glyph_read` 回调(轴3)——各归其位，方案切换不改控件代码。
- **目录对齐前作 YMCV**：可移植的 9 层全收在 `YMGUI/` 库根下（对齐 YMCV 的 `OpenSrc-YMCV/YMCV/`，拷一个文件夹即移植）；CONFIG/DEBUG/COMMON/OPOBJ/CORE 与 YMCV 同名，GUI/HAL/WIDGET/STATE 为 GUI 新增。

## 维护约定（重要）

文档是快照，会随代码过期。**每轮加控件/改机制时顺手更新三处**：
- `API.md` — 新控件的函数
- `DEVLOG.md` — 这轮做了啥 + 踩的坑
- `PROJECT.md` — 控件计数 / 进度快照

`Demo/test_*.c` 里的断言即行为规格 —— 改机制先看对应测试，改完让它继续过。
