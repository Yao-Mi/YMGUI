# 音乐工坊曲库 · 第一辑

五首作品、七个可编辑的 `.ymmusic` 工程，全部使用音乐工坊现有采样音源。音符、力度、声部和时间线片段可以修改，也可以替换音色、调速度或导出 WAV。

## 打开试听

1. 在音乐工坊点左上角 **打开**，进入本目录的 `projects/`，选一个 `.ymmusic` 文件。
2. 按空格播放整曲。点素材库中的乐句，再点 **试听片段**，可单独循环练习；顶部模式按钮可以切回整曲。
3. 修改后用 **另存为** 保存自己的版本。**导入音频** 按钮用于 WAV/MP3，曲库工程使用 **打开**。

原始曲库目录：`/home/yaomi/YMGUI/project_Demo/music_studio/library/projects/`。本机已复制一套供试听和修改的工程到 `build/rgb565/project_Demo/music_studio/user/曲库试听/工程/`，并在同级生成 `音乐工坊曲库-第一辑.zip`（含工程、来源、说明与重制工具）。

| 工程 | 配器 | 时长 | 内容 |
| --- | --- | --- | --- |
| [01 致爱丽丝](projects/01-致爱丽丝-钢琴主题节选.ymmusic) | 钢琴双手合轨 | 28.13 秒 | 开头主题节选 |
| [02 土耳其进行曲](projects/02-土耳其进行曲-钢琴节选.ymmusic) | 钢琴左右手分轨 | 25.71 秒 | 开头节选 |
| [03 G大调小步舞曲](projects/03-G大调小步舞曲-钢琴全曲.ymmusic) | 钢琴双手合轨 | 48 秒 | 源 MIDI 全曲 |
| [04 G大调小步舞曲](projects/04-G大调小步舞曲-室内乐节选.ymmusic) | 长笛旋律＋弦乐伴奏 | 24 秒 | 前半段改配器 |
| [05 绿袖子](projects/05-绿袖子-吉他全曲.ymmusic) | 吉他旋律与和声 | 48.50 秒 | 源 MIDI 全曲 |
| [06 欢乐颂](projects/06-欢乐颂-合唱全曲.ymmusic) | 高、低合唱声部分轨 | 38.40 秒 | 四声部主题编配的完整 MIDI |
| [07 欢乐颂](projects/07-欢乐颂-铜管全曲.ymmusic) | 铜管＋圆号 | 35.56 秒 | 同一主题编配，改配器 |

“全曲”指所用 Mutopia MIDI 的全部音符，不额外展开源文件没有播放的反复记号；《欢乐颂》是该站的 SATB 主题编配，非整部第九交响曲。合唱使用元音采样，没有歌词演唱。

本机另备七份实际导出的 WAV，位于 `build/rgb565/project_Demo/music_studio/user/曲库试听/`。这些 WAV 是我们的采样引擎输出，工程本身不嵌入录音；总计 7 个工程不到 70 KB。播放需使用带随附音源的当前版本音乐工坊。

## 来源与许可

2026-09-20 从 [Mutopia Project](https://www.mutopiaproject.org/) 下载。下列五份版本的上游元数据均标记 **Public Domain**；LilyPond 源文件或源压缩包、RDF 许可元数据保留在 `sources/`；原 MIDI 仅保留本地，不纳入 Git。原始文件的下载 URL 和 SHA-256 仍保存在 [来源清单](sources/manifest.json)，便于按需恢复重制输入。采样音源的许可仍按应用随附的音源说明执行。

| 作品 | 作曲者 / 传统来源 | Mutopia 排版者 | 官方下载目录 |
| --- | --- | --- | --- |
| 致爱丽丝 WoO 59 | 贝多芬 | Stelios Samelis | [原版](https://www.mutopiaproject.org/ftp/BeethovenLv/WoO59/fur_Elise_WoO59/) |
| 土耳其进行曲 K.331 第三乐章 | 莫扎特 | Rune Zedeler、Chris Sawer | [原版](https://www.mutopiaproject.org/ftp/MozartWA/KV331/KV331_3_RondoAllaTurca/) |
| G大调小步舞曲 BWV Anh.114 | 佩措尔德；上游仍归在 BachJS 目录 | Allen Garvin | [原版](https://www.mutopiaproject.org/ftp/BachJS/BWVAnh114/anna-magdalena-04/) |
| 绿袖子 | 传统曲调 | Aaron Fontaine | [原版](https://www.mutopiaproject.org/ftp/Traditional/Greensleaves/) |
| 欢乐颂主题 | 贝多芬，SATB 编配 | Peter Chubb | [原版](https://www.mutopiaproject.org/ftp/BeethovenLv/ode/) |

转换改动：选择节选范围；按 [配器和速度配置](catalog.json) 替换 MIDI 音色、调整固定速度、轨道音量和声像；部分钢琴双手合并到同一轨；将音符映射至 96 PPQ 并组织为可编辑乐句。不复制第三方录音，不新增伴奏音符。

## 转换边界和重制

- 现有工程上限为 16 个乐句、每句 128 音符、每句最多 8 个四分音符。本辑每个工程使用 8–14 个乐句。长曲明确节选；不修改应用容量或工程格式。
- 乐句按完整音符装箱，必要时让时间线片段重叠；不在内部乐句边界切断/重触发延音。每次转换展开所有片段，与选中 MIDI 音符逐项比对；节选外的内容不写入工程。
- 固定速度，保留原音高、相对时值及力度，时间舍入至 96 PPQ。原 MIDI 的踏板/控制器、原乐器程序和速度图不作为播放状态导入；检测结果记录在 [转换报告](projects/conversion.json)。本辑只有《致爱丽丝》包含三个控制器事件。
- 《致爱丽丝》原为 3/8，当前界面只表示以四分音符为拍的分子，因此其 tick 与 BPM 同时乘二，显示为 3/4，实际音符时长不变。其他曲目依原拍号；弱起从时间线零开始，没有额外的弱起小节标记。
- `build_library.py` 是这一辑的离线转换工具，支持所选标准 MIDI 的格式 0/1、PPQ 和音符事件，不是应用内通用 MIDI 导入功能。未知系统事件、损坏/不成对音符、单音超过八拍或工程超容量明确报错。

转换脚本已保存在 [tools/build_library.py](../tools/build_library.py)，配置固定读取本曲库的 [catalog.json](catalog.json)，资源固定读取 `sources/`。仅需 Python 3 标准库，离线运行；无需安装 MIDI 包、编译应用或重新下载音源。脚本通过自身位置定位资源，可以从其他工作目录启动。

正常打开、播放和导出已有 `.ymmusic` 工程不依赖原始 MIDI。新检出后若要运行转换脚本或完整 Python 转换回归，先从 `sources/manifest.json` 中各 `.mid` 条目的 `url` 下载文件，按对应 `file` 名称放回 `sources/`；转换器会核对 SHA-256。缺少源文件时无法重制，脚本不会自动联网下载。

准备好原始 MIDI 后，在仓库根目录运行：

```bash
python3 project_Demo/music_studio/tools/build_library.py
python3 project_Demo/music_studio/tests/test_library.py
# 输出到其他目录；已存在且内容不同的工程会拒绝覆盖
python3 project_Demo/music_studio/tools/build_library.py --output /tmp/音乐工坊曲库
# 播放其中一个工程
./build/rgb565/project_Demo/music_studio/music_studio \
  --project project_Demo/music_studio/library/projects/06-欢乐颂-合唱全曲.ymmusic --tab 1
```

不传 `--output` 时写入 `library/projects/`，生成每个曲目的 `.ymmusic` 和一份 `conversion.json`。`--output` 仅改变输出目录，不改变配置或 MIDI 来源；相对输出路径相对于当前工作目录。已有工程与生成内容相同可重复运行，内容不同则报错并保留该工程；整批转换不是原子操作，报错前生成的其他文件会留下。

脚本只生成原生工程和转换报告，不会自动导出 WAV、复制用户试听副本或打包 ZIP。本机的这些试听产物在准备曲库时另行生成；更新曲库后需用音乐工坊重新打开、试听和导出。

## 配置字段与新增曲目

`catalog.json` 是曲目对象数组；每个对象对应一个输出工程。同一 MIDI 可以配置多个节选或配器版本。

| 字段 | 含义 |
| --- | --- |
| `title` | 工程名及输出文件名主体；使用不含路径分隔符的名称，UTF-8 编码小于 64 字节 |
| `source` | `sources/` 下的 MIDI 文件名；当前没有单文件 `--input` 参数 |
| `start_quarters`、`end_quarters` | 源 MIDI 的起止位置，单位为四分音符；起点可省略，默认 0，终点必填 |
| `tick_scale` | 音乐时间缩放，默认 1；若只为调整界面拍号表示，应同比缩放 BPM，以保持实际音长 |
| `bpm`、`beats` | 输出的固定速度、每小节四分音符拍数；工程支持 BPM 30–300、拍数 1–12，不存拍号分母 |
| `master` | 主音量，默认 0.8；工程允许 0–1.5 |
| `tracks` | 输出轨道列表；本转换器生成乐器轨，工程最多 8 轨 |
| `tracks[].name` | 中文轨道名，UTF-8 编码小于 64 字节 |
| `tracks[].midi_tracks` | 源文件中从 0 开始的 **MIDI 轨道编号**，不是 MIDI 通道号；多个编号会合并到同一输出轨 |
| `tracks[].instrument` | 音乐工坊的采样音色编号，范围 0–12 |
| `tracks[].volume`、`tracks[].pan` | 轨道音量、声像；默认 0.85、0，允许范围分别为 0–1.5、-1–1 |

音色编号：0 钢琴、1 电钢琴、2 贝斯、3 吉他、4 弦乐、5 管风琴、6 长笛、7 电子主音、8 圆号、9 铜管、10 定音鼓、11 人声元音、12 合唱。编号以 [模型定义](../model/project.c) 为准，不使用 General MIDI 的乐器编号。

新增曲目时，先在本地准备有明确使用许可的 MIDI，并保留乐谱及来源元数据；原始 MIDI 由忽略规则排除，不提交。在 `sources/manifest.json` 添加对应文件的 `file`、`url`、`sha256`，再在 `catalog.json` 添加曲目及声部配置。轨道选择、节选位置与速度需要人工核对；转换器不自动判断曲式或选择配器。

修改配置后，先用 `--output` 输出到新目录，检查报告中的时长、音符数和容量，再用当前版本音乐工坊打开并导出试听。配置表中的范围来自应用模型，不能把脚本生成成功视为已通过应用全部校验。定稿后同步曲目表、转换报告及 [转换回归](../tests/test_library.py) 的预期曲目数量；维护工程格式时还需验证 C 加载器保存重开一致。

验证：七份工程经应用 C 加载器校验、保存重开并比较模型，均一致；全部通过本应用离线导出，48 kHz / 16 位 / 立体声、非静音、无削波。六项 Python 回归覆盖 MIDI running status、零力度 note-off、坏数据、跨乐句延音、共享乐句、容量限制、逐字节重制和保护已有编辑；测试不替代专业演奏效果验收。
