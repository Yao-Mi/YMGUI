# YMGUI 动效场景示例

这二十个示例演示的是 **YMGUI 控件树和交互的动效**，不是 YMANIM 的矢量动画集锦。十九个只用标准 YMGUI 对象/控件与 `YMGUI_Anim_*`；仅播放头示例使用 Canvas 像素缓冲。它们均可独立运行，命令行参数是最多运行的帧数，适合无头截图。

| 程序 | 交互 | 重点 |
| --- | --- | --- |
| `demo_anim_drawer` | Menu、Overview/Activity、Close | 容器连同子控件一起滑入滑出 |
| `demo_anim_dialog` | Configure、Cancel/Apply | top layer 模态遮罩与弹窗入场/退场 |
| `demo_anim_cards` | Stagger、Reorder | 延迟错峰入场、卡片重排和颜色补间 |
| `demo_anim_progress` | Start、Reset | 整数补间驱动 Bar、百分比和阶段反馈 |
| `demo_anim_canvas` | Replay、Pause | 唯一的 Canvas：播放头和进度条同步 |
| `demo_anim_tabs` | Overview/Activity/Settings | 页签指示条与三个真实页面同步横移 |
| `demo_anim_accordion` | 点击三个分区标题 | 面板尺寸变化与后续行位置联动 |
| `demo_anim_toast` | Notify、Clear | 三条通知错峰入场、反向退场 |
| `demo_anim_scroll` | Top、Next、条目 Open | 带子控件和裁剪的容器平滑滚动 |
| `demo_anim_theme` | Ocean、Sunset | 根背景、面板和色块协同变色 |
| `demo_anim_focus` | 点击输入框、按 Tab | 焦点框跟随真实 TextInput 焦点移动 |
| `demo_anim_validation` | 编辑邮箱、Validate、Fix sample | 无效输入回弹并变色，修正后显示通过 |
| `demo_anim_batch` | 勾选条目、Clear、Archive | 选中态驱动底部批量操作栏入退场 |
| `demo_anim_list_ops` | Add、Remove、Reorder | 真实对象增删、相邻行让位/合拢及重排 |
| `demo_anim_drag_snap` | 拖动卡片、Next | 指针捕获跟手，松开后吸附最近位置 |
| `demo_anim_submit` | Submit / retry | 按钮尺寸和颜色、Bar 进度与完成态串联 |
| `demo_anim_loading` | Load、Reset | 占位块收起，内容卡片错峰进入 |
| `demo_anim_detail` | 选择条目、Close | 列表收窄，侧边详情滑入或退出 |
| `demo_anim_pull_refresh` | 下拉列表、Refresh | 跟手拖拽、阈值触发、保持、更新与回弹 |
| `demo_anim_carousel` | 左右滑动、Previous / Next | 手势切页，卡片吸附到最近页 |

```bash
cmake -S . -B build/rgb565/Demo -DYMGUI_COLOR_DEPTH=16
cmake --build build/rgb565/Demo --target demo_anim_list_ops demo_anim_drag_snap demo_anim_pull_refresh demo_anim_carousel -j8
./build/rgb565/Demo/demo_anim_drawer
SDL_VIDEODRIVER=dummy ./build/rgb565/Demo/demo_anim_cards 40
SDL_VIDEODRIVER=dummy ./build/rgb565/Demo/demo_anim_drag_snap --selftest
```

二十个程序都在启动时播放一次动画，无头截图无需注入鼠标事件。桌面窗口中可用按钮、输入框、勾选框或指针拖拽反复触发。拖拽卡片、下拉刷新和轮播吸附也提供按钮触发同一动画，便于无触摸屏时预览。窗口采用 1:1 像素显示，避免 SDL 将小屏 framebuffer 最近邻放大两倍造成粗锯齿；截图仍是原生 480×272。SDL 桥接层已按真实经过时间自动注入 `Tick`，示例主循环不再手动推进时钟；移植到裸机时由平台时钟调用 `YMGUI_Inject_Tick(elapsed_ms)`。原有 [`demo_anim.c`](demo_anim.c) 展示底层补间 API。

本轮新增的七个示例都接受 `--selftest`，通过输入注入检查快速重复点击、动画打断、真实拖动与最终吸附/回弹，无需显示器。`tools/test_anim_interactions.sh` 会在 RGB565 / RGB888 下构建并执行全部七项及动画核心回归。

`ANIM` 是编译期可裁剪模块，默认开启。关闭时不编译 `YMGUI/ANIM/`，不创建动画 Demo 或 `test_anim` 目标，HAL 的普通 Tick 和其他控件照常工作：

```bash
cmake -S . -B build/rgb565/anim-off -DYMGUI_COLOR_DEPTH=16 -DYMGUI_ANIM=OFF
cmake --build build/rgb565/anim-off --target demo_button -j8
tools/test_anim_trim.sh
```

移植源码时也可在配置头或编译参数设置 `YMGUI_ANIM=0`，此时动画 API 不提供，`GYctx` 不再保留动画专用管理指针。`project_Demo/ymgui_app.cmake` 同样接受 `-DYMGUI_ANIM=OFF`；预编译 SDK 必须使用与库一致的开关，不能在消费端单独裁掉。
