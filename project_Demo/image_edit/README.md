# image_edit —— 类 PS 图层画板(第四个基础验证项目)

`project_Demo` 的第四个"真实小应用验证库缺口"的项目。延续约定:txt_edit 催生 `EditView`、
files_manager 催生 `TreeView`、excel_edit 催生 `Grid`,**image_edit 催生两个库控件
`Canvas`(可绘制位图视口)+ `ColorPicker`(HSV 取色)**。

## 分层

- **库侧 `YMGUI_Canvas`**:通用可绘制位图视口 —— 自持 `cw*ch` 行优先 `GYpx` 显示缓冲
  (`GetBuffer` 直写)、整数倍缩放(1..16)、平移/手型平移、屏↔画布坐标映射(负坐标 floor
  除,越界不误判)、绘制回调(DOWN/MOVE/UP + in_bounds)、`IsDrawing`(供喷枪逐帧驱动)。
  **库不认识"图层/融合"**。
- **库侧 `YMGUI_ColorPicker`**:HSV 取色器 —— SV 方块 + 色相条,整数 HSV↔RGB(无 libm/FPU),
  拖动即回调。**库不认识"当前墨色/工具"**。
- **app 侧(本项目)**:图层栈、混合模式、合成、各绘制工具全在这里(与 Grid 不懂公式、
  TreeView 不懂文件系统一致)。

## 图层模型(app 侧)

- 每图层 = RGBA 直存(直 alpha,非预乘),行优先 `CW*CH`,独立 **可见位 / 不透明度 / 混合模式**。
- 合成:棋盘底(16px 格)→ 自底向上叠各可见图层。有效 alpha = 像素 alpha × 图层不透明度;
  先按混合模式算叠加色再按 alpha 线性插值。写入 Canvas 显示缓冲(`g_dirty` 脏标记,变了才重合成)。
- 混合模式:正常 / 正片叠底 / 滤色 / 线性减淡(单通道整数实现)。

## 工具

- **硬笔刷**:半径内实心圆,coverage=255。
- **橡皮**:按 coverage 削减 alpha(RGB 不动)。
- **柔和笔刷**:coverage 随到圆心距离线性衰减(整数 ceil(sqrt) 近似) → **边缘渐隐/过渡感**。
- **喷枪**:固定喷口按粒度把 N 个散点落在半径内。**粒度越细 → 每点 coverage 越低 → 直 alpha
  累积越柔和、色度越低**;越粗 → 每点越实。按住不动靠 `IsDrawing` 逐帧持续喷(MOVE 不触发时)。
- **直线 / 矩形 / 三角形 / 圆形**:DOWN 记起点,**拖动实时预览**,UP 定形。三角形取拖框上边中点为
  顶点、底边两角;圆形以起点为圆心、到当前点距离为半径(中点画圆,整数 `isqrt` 免 FPU)。
- **折线 / 多边形**:逐次点击加顶点,橡皮筋预览随光标;**点回起点附近(8px 内)且够点数**(折线≥2、
  多边形≥3)则结束/闭合并提交。切工具丢弃未完成顶点。
- **填充桶**:4 邻域 flood fill(静态栈免递归),按 app 侧图层色 + alpha 容差匹配连通域。
- **吸管**:从合成结果取色 → 设墨色 + 同步 ColorPicker。

线段/拖动用 `strokeTo` 沿上次点→当前点补点(步长 ≈ rad/2)防断线。

### 形状实时预览机制

形状栅格化与"落点动作"解耦:`rasLine/rasRect/rasTri/rasCircle/rasPoly` 都收一个 `PlotFn` 回调。
`plotLayer` 把实心圆盖到当前图层(提交);`plotPreview` 直接把墨色写到 Canvas 显示缓冲(预览)。
拖动中每次 MOVE 先 `composite()` 重建底图,再叠预览形状、**不置 `g_dirty`**(预览留屏直到下次事件),
抬起才真正提交到图层。

## UI

- **左面板**:ColorPicker + 工具下拉 + 混合模式下拉 + 笔刷尺寸/喷枪粒度/**选中层**不透明度滑块 +
  显隐/清空选中层按钮。
- **中间**:Canvas 视口(320×240 画布,放大 2×)。
- **右侧图层缩略图列表**(app 侧自绘对象,单 `draw_cb` 画全部行 + `event_cb` 命中):行序 = PS 序
  (顶层在最上),每行 = 降采样缩略图(叠棋盘底)+ 层号/混合/不透明度 + 眼睛显隐块。**点行选中该层**
  (才把不透明度/混合/显隐等属性同步到该层);点眼睛切显隐。面板下方 **＋新建 / －删除** 图层
  (至少保留 1 层;删除后上层内容前移)。

GB2312 全字库回退。

## 构建 / 运行

```sh
# 从仓库根:
cmake -S project_Demo/image_edit -B build/project_Demo/image_edit
cmake --build build/project_Demo/image_edit -j

# 窗口运行:
./build/project_Demo/image_edit/image_edit

# headless 自检(argv[1]=帧数;判成败以 exit code 为准):
SDL_VIDEODRIVER=dummy ./build/project_Demo/image_edit/image_edit 3
# 打印 "selftest: all tools + compositing OK" + "image_edit exit ok",exit 0
```

自检(14 项)覆盖:硬笔刷实心/圆外不溢、柔和笔刷中心>边缘、橡皮清 alpha、喷枪粒度(粗>细,同种子
可复现)、直线端点/中点、矩形四角/内部空、**三角形顶点+底角、圆形边缘处被涂且圆心不描边、多边形三
顶点闭合**、填充桶铺满 + 上色、合成取色一致、半透明层向底靠拢、隐藏层不参与合成、**增/删图层(计数、
内容前移、末层不可删)**。

## 已知取舍

- 定容图层(`CW=320 CH=240`,最多 `MAX_LAYERS=4`),不做动态分辨率/无限图层。
- 无撤销栈(库 EditView 有 undo,这里画板未接;工具即时落笔)。
- flood fill 容差固定 24;吸管从量化后的 RGB565 合成缓冲取色(有量化误差)。
- 形状预览每次 MOVE 全画布 `composite()` 重建底图(定容 320×240,开发机足够;真机可改为局部脏区)。
- 折线/多边形靠"点回起点附近"闭合(Canvas 回调只有 DOWN/MOVE/UP,无双击/右键语义)。
- 图层缩略图列表是 app 侧自绘对象;若后续多个项目都要"可选中缩略图列表",可再抽成库控件
  (延续 EditView/TreeView/Grid/Canvas 的"真实应用暴露库缺口"路线)。
