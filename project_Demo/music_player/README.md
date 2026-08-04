# music_player —— 音乐播放器(第五个基础验证项目)

`project_Demo` 的第五个"真实小应用验证库缺口"的项目。延续约定:txt_edit 催生 `EditView`、
files_manager 催生 `TreeView`、excel_edit 催生 `Grid`、image_edit 催生 `Canvas`+`ColorPicker`,
**music_player 催生两个通用库控件 `Roller`(居中高亮平滑滚动列表)+ `BarChart`(通用柱状图)**。

用户明确要求控件**不做"歌词专用"**:要一个能复用的东西(平滑滚动 + 中间行高亮),歌词/时间/日历/
拨码都能用,再在其上动态显示歌词;频谱同理——不叫 Spectrum,造一个通用**柱状图**(带顶部回落 + 两种
配色),频谱只是它的一种用法。所以库里落地的是通用 Roller / BarChart,"歌词/频段"语义留在 app 侧。

## 分层

- **库侧 `YMGUI_Roller`**:通用居中高亮平滑滚动列表 —— 选中行永远绘制在控件竖直正中并高亮,其余行
  上下排开、越界裁掉;滚动位置 16.16 定点,每帧 `Tick` 向目标行缓动逼近 → 平滑滚动。程序态(只
  `SetSelected`)/ 交互态(拖动 + 吸附)两用。**库不认识"歌词"**。
- **库侧 `YMGUI_BarChart`**:通用柱状图 —— N 柱按值映射高度。两个正交维度组合出多种用法:配色
  `BY_HEIGHT`(随高度 lo→hi 渐变,同一物理量不同强度)/ `PER_BAR`(每柱独立调色板,不同类目);
  顶部回落 `TOP_BAR`(高亮细条峰值保持 + 每帧回落,频谱手感)/ `TOP_NONE`(纯柱)。`SetDecay(0,0)`
  + `TOP_NONE` = 一张静态普通柱状图。**库不做频段/统计分析,值由外部喂**;文字顶标交给 app 叠 Label
  (库不引 Font 依赖)。此处配 `BY_HEIGHT + TOP_BAR` 当频谱用。
- **app 侧(本项目)**:解码 / 声卡输出 / 播放时钟 / seek / 歌词解析 / 频段分析全在这里(与 Grid
  不懂公式、Canvas 不懂图层一致)。

## app 侧模块

- **mp_audio**(`.h/.c`)—— 播放引擎。**SDL2 音频只出现在这一个文件,不进 GUI 库**。
  - **解码**:无 ffmpeg 开发库,走 **popen ffmpeg CLI** 把任意音频整曲解成 `44100Hz / 立体声 /
    s16le` 裸流读进内存(`ffmpeg -nostdin -v error -i '<path>' -f s16le -ac 2 -ar 44100 -`,
    路径单引号转义防命令注入)。整曲进内存 → seek/进度/频谱都在一块缓冲上做,seek 即改帧游标。
  - **输出**:SDL2 `SDL_QueueAudio`(queue 模式,非 callback),每帧维持约 0.3s 队列水位。可闻位置
    = 已推送帧 − 声卡队列剩余(`SDL_GetQueuedAudioSize`)。
  - **静音兜底**:开设备失败(headless / 无声卡)→ 不出声,播放位置改由 wall-clock(`SDL_GetTicks`)
    推进,UI 逻辑不变。
  - **合成兜底**:无文件 / 解码失败 / ffmpeg 不可用 → 合成一段 C 大调音阶琶音(库 Q15 `GY_Sin` +
    三角窗包络 + 低八度叠加,纯整数),引擎仍可播,便于无素材演示。
- **mp_lrc**(`.h/.c`)—— `.lrc` 歌词解析。`[mm:ss.xx]` 时间标签,一行多标签共享文本,按时间排序
  插入,二分查"播放毫秒 → 应高亮行索引"。
- **mp_spectrum**(`.h/.c`)—— 频段能量分析。整数 **Goertzel**(无 libm):中心频率二次分布近似对数
  (低频密、高频疏),`coeff=2*cos(w)` 用库 Q15 三角表,幅度用整数 `isqrt64`,高频段增益补偿。目标是
  "能动、跟音乐相关",不追真 FFT 精度(定调:库核心是 GUI,不做音视频分析)。

## UI(800×480)

- **上方**:BarChart 当频谱(24 柱,`BY_HEIGHT` lo 蓝→hi 品红渐变 + `TOP_BAR` 峰值标记)。每帧从
  当前播放点取一窗 mono 样本 → `mp_spectrum_analyze` → `SetValues` 喂柱。
- **中部**:Roller 歌词(5 可见行,居中高亮)。每帧按播放毫秒 `mp_lrc_index_at` 求当前行,
  `SetSelected(idx, animate=1)` 平滑滚到中间。无 `.lrc` 时用内置默认词演示滚动。
- **下方**:可拖动进度 Slider(range 0..1000,`changed` 回调换算毫秒 `seek`;拖动中靠
  `slider->state & GY_STATE_Pressed` 判定,时钟不回写防抖)+ 当前/总时长标签 + 播放/暂停按钮。

GB2312 全字库回退(复用 `Demo/gb2312_glyphs.bin`),中文歌词/标题可显示。

## 构建 / 运行

```sh
# 从仓库根:
cmake -S project_Demo/music_player -B build/project_Demo/music_player
cmake --build build/project_Demo/music_player -j

# 窗口运行(自动开播;不带参数时用工程自带的默认曲目 可能-队长.mp3 + .lrc):
./build/project_Demo/music_player/music_player

# 指定音频文件 + 歌词:argv[2]=音频, argv[3]=.lrc
./build/project_Demo/music_player/music_player 0 song.mp3 song.lrc
./build/project_Demo/music_player/music_player 0 /path/to/song.flac project_Demo/music_player/sample.lrc

# headless 自检(argv[1]=帧数>0 触发 selftest;判成败以 exit code 为准):
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ./build/project_Demo/music_player/music_player 30
# 打印 "selftest: lrc + spectrum + audio engine OK" + "music_player exit ok",exit 0
```

自检(3 组)覆盖:LRC 解析(标签数 / 按时间排序 / 文本 / 播放毫秒查询 / 越界钳制)、频谱(纯正弦有
能量 / 静音全 0)、音频引擎(合成兜底时长 / seek 位置 / seek 越界钳制 / peek 取样)。

## 已知取舍

- **整曲解码进内存**:3 分钟立体声 ≈ 31MB,开发机足够;真机 RAM 紧张可改流式解码 + 环形缓冲。
- **依赖系统 ffmpeg CLI**(`popen`):机器上要有 `ffmpeg` 可执行。没有则自动回落到合成旋律。
- **频谱是整数 Goertzel 近似**:柱数 = 频段数,不是真 FFT bin;够"跟着音乐跳",不做精确频率分析。
- **进度条拖动检测借 `state` 位**:库 Slider 无专门"拖动开始/结束"事件,用 `GY_STATE_Pressed`
  近似;拖动松开后下一帧时钟接管。
- **无播放列表 / 音量 / 循环模式**:单曲演示为主,聚焦验证 Roller / BarChart 两控件够不够用。
- Roller / BarChart 都是通用控件;若后续项目还要"平滑滚动选择器 / 柱状可视化(直方图/统计/电平表)",
  直接复用(延续 EditView/TreeView/Grid/Canvas 的"真实应用暴露库缺口"路线)。
