# AT32 手机移植回流说明

本轮对照用户的 `ymgui_phone`（AT-SURF-F437、ARMCC 5）与当前 YMGUI 源码，吸收硬件无关的修复和优化。原工程只读且保留于来源目录；本仓库不复制板级 SDK、型号实现、调试脚本或硬件日志。当前 phone 与移植工程的应用逻辑合并为一份共用代码。

| 移植中的经验 | 库中的结果 | 适用条件 |
| --- | --- | --- |
| 离屏动画扩大刷新面积 | 合并前屏幕裁剪、祖先可见性裁剪、动画属性未变不标脏 | 默认开启；越界自绘/子树采用保守范围 |
| 小宽度脏区配整屏 buffer | band 行数先以 uint32 钳制后转换 | 默认修复，防止 int16 溢出 |
| 启动层/窗口遮住底层重复绘制 | 可选不透明覆盖计划和最多四段可见 clip | `YMGUI_OCCLUSION=1`；回调必须兑现绘制承诺 |
| SD 字形在不同 band 重复读取 | 字形 resolve 后、读 bitmap 前检查 clip/buffer | 默认开启；保留实际 fallback advance |
| RGB565 逐像素混合成本 | 等价整数除法、背景像素缓存、memcpy 批量填充 | RGB565；其他色深保留原混合 |
| 图片重复背景填充/逐点裁剪 | 行拷贝、源列缓存、同尺寸直拷、只填 letterbox | 不透明图片可快拷；透明色保留背景与跳过语义 |
| 五触点同时输入 | 完整 Touch 帧、稳定 ID、单指捕获取消和监听器 | 应用监听器实现手势；SDL 已适配完整触点帧 |
| 中文输入丢字/连续退格越界 | 清理塌缩/失效锚点，修改后重置锚点，静止拖动不建选区 | TextInput 和 EditView，逐字节 UTF-8 输入 |
| 32 KiB 静态剪贴板占用片内 RAM | 可选内存区 1 按需分配和 Clear 释放 | `YMGUI_CLIPBOARD_DYNAMIC=1`；默认仍静态 |

根工程和 `project_Demo/ymgui_app.cmake` 提供同名 CMake 选项，默认 OFF。直接复制 `YMGUI/` 时定义同名 C 宏为 1。宏不会改变 GYobj/GYctx/GYdisp 的字段或 ABI；新增函数仍需重新编译库，旧预编译包没有它们。

```bash
cmake -S . -B build/checks/at32/rgb565 \
  -DYMGUI_COLOR_DEPTH=16 -DYMGUI_OCCLUSION=ON -DYMGUI_CLIPBOARD_DYNAMIC=ON
cmake --build build/checks/at32/rgb565 -j8
ctest --test-dir build/checks/at32/rgb565 --output-on-failure
```

RGB888 使用独立 `build/checks/at32/rgb888` 目录及 `-DYMGUI_COLOR_DEPTH=24`。`test_port` 将优化开启/关闭后的 framebuffer 逐字节比较，覆盖不透明并集、透明叠加、未知阴影自绘、祖先滚动裁剪、多 band、容量溢出、确定性随机重叠、填充/字形像素、图片 buffer 边界、实际 HAL 多指取消、两种编辑器的中文折返选区及重复退格、脏区和大容量窄区刷新。

遮挡计划默认关闭，不占绘制计划工作区。开启时工作区在当前 Linux x86_64、16 位坐标构建中为 9736 字节（`nm -S` 的 draw_plan），具体 MCU/坐标宽度需看链接 map。`OpaqueRect` 要求完整不透明矩形覆盖并且不在 abs 外绘制；`BoundedDraw` 只保证不越界，可半透明。不要给圆角、稀疏字形、透明图片或越界阴影设置 OpaqueRect。各 draw_cb 必须可在同一帧重复调用，仅产生绘制结果，不能修改对象树或递归刷新；全局工作区与诊断计数不支持并发刷新。容量不足时保守多画，不能漏画。

RGB565 的批量填充采用 memcpy，避免照搬 uint16 buffer 转 uint32 指针写入所带来的有效类型和对齐假设。图片的固定列缓存使用 GYcoord，避免把源位置限定为 uint16；可见宽度超过 320 时保留逐点采样。

第二轮补齐了第一轮没有回流的应用和资源实现：

| 原工程实现 | 当前落点 | 复用方式 |
| --- | --- | --- |
| 壁纸按主题缓存、失败退回绘制 | phone_shell | 默认开启；不改变主题切换结果 |
| 每应用缩略图、共用全尺寸工作缓冲、缓存采样轴 | phone_shell + ScaleBilinear.h | 120×168 预览；双线性默认，也可选最近邻；离开前提交 |
| 固定状态栏、最近卡片圆角、位置变更擦除旧范围 | phone_shell + phone_ui | 同一份桌面/MCU 应用代码 |
| 相册稳定双指 ID 缩放 | apps/gallery.c + TouchGesture.h | 真正由 HAL 完整帧派发；100–300%，失败回退 |
| 中文触屏文字操作菜单 | WIDGET/TextActions | 可选择启用，按根宽和可用高度布局、翻译回调、目标销毁解绑；TextInput/EditView 共用 |
| 输入法候选池及组合状态搬到外存 | chinese_ime/ime_candidates.inc + phone_ime | memory1 动态工作区，初始化失败回收，退出释放；字典通过 Resource 加载 |
| FatFs 资源读取 | HAL/Resource；文件系统回调由目标项目实现 | 后端回调复制，打开文件保存各自后端；关闭/完整读取失败路径统一；stdio 可关闭 |
| 小片内 FIFO + 可选外存 LRU 字模缓存 | CORE/FontCache + phone_locale | 参数化容量；32/128 项；短读空白、溢出重置、分配失败降级、校验字库两端/长度 |
| SDRAM 分裂/合并堆 | CONFIG/Arena；目标项目提供存储区 | 调用方提供存储区；保留 realloc 失败原数据，统计 used/peak/最大连续空间 |
| GUI/Worker/LCD 总线所有权 | HAL/Bus | critical/ISR 回调；LCD 完成归还 GUI，工作任务延后；保留原 AT32 实现供核对 |
| 片内 SRAM 分块发送 | HAL/StagedFlush + 原 phone_board.c | 可复用同步算法，回调必须完成或中止 DMA 后返回；AT32 参考保留异步完成/超时/扫描同步细节 |
| 具体触摸型号解包 | 原移植工程；目标平台输入适配 | 仓库只保留通用完整触点帧、稳定ID和取消机制 |
| 两键导航与共用启动/刷新/清理 | phone_runtime.h | 无 SDL 静态目标与桌面共用；Step 或已注入 Tick 的 Pump；Key 支持选择/点击/返回/桌面 |
| 硬件访问冲突、扫描同步、板上诊断 | 原移植工程 | 不复制具体实现；通用所有权和分块算法已回流，硬件策略由目标平台决定 |

应用优化保留在共用 phone，经过实机验证的通用机制进入库，具体设备协议、寄存器、BSP和诊断留在来源工程及目标平台自己的适配层。本仓库不保留 ports 目录。SDL与其他后端都通过相同Touch帧及显示/资源/内存回调接入；生成对象/固件、旧SDK路径和已撤回实验不引入。

phone 快照当前开销：每应用 RGB565 39.375 KiB / RGB888 59.0625 KiB；共用 320×448 工作缓冲 280/420 KiB，主题壁纸 300/450 KiB，双线性采样表 3456 字节，亮度缓冲按显示 buffer 容量，截图和相册缩放另按需分配。缓存失败自动回退，不再为每个后台应用保留全尺寸画面。缓存表和工作缓冲在最后一个预览释放或 Shutdown 时回收。

无 SDL 编译与平台接入见 [跨平台移植](CROSS_PLATFORM_PORTING.md)。`test_port_support` 覆盖堆碎片回收/realloc、ISR/所有权/分块失败、资源后端生命周期、字模缓存/短读/计时回绕和采样边界。`phone_portable` 验证反复 Init/Shutdown、HAL双指缩放、局部与完整渲染等价、文字菜单及目标销毁；SDL回归验证PC适配后的相同输入机制。

本轮验证只在主机执行，不包含 AT32 固件重新编译/烧录或板上触摸与刷新验收。原 README_PORT 中的板上性能、时序与故障记录不能作为当前代码的硬件验收结论。现有 `releases/` SDK 未重新生成。

2026-10-08 SDK 已更新至本轮回流基线，包含通用接口及匹配的双色深核心/SDL静态库。可选遮挡和动态剪贴板采用通用默认关闭配置，五触点及默认stdio开启；定制配置需从源码重建。实际压缩包通过双色深示例与动画/移植/资源/SDL回归，详见 [分发说明](../releases/README.md)。
