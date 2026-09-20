# music_studio 全采样音源

这是供 `project_Demo/music_studio` 使用的音源数据，不是 YMGUI 核心依赖。

来源为 S. Christian Collins 的 [GeneralUser GS](https://github.com/mrbumpy409/GeneralUser-GS)，固定提交 `684543d5e5efaef08d02be50dcda8d552478fa60`，版本 2.0.3。通过 FluidSynth 2.2.5 离线制作专用 PCM bank 和鼓声 WAV。完整许可保留在 [LICENSE.txt](LICENSE.txt)，上游说明保留为 [UPSTREAM_README.txt](UPSTREAM_README.txt)。资源是派生数据，作者不为本应用的转换或播放效果背书。

应用中的全部 13 种乐器和 9 种鼓声均播放采样；没有特征再合成或实时乐器振荡器。电子主音是电子音色的 PCM 采样，不能将“全部采样播放”理解为全部来自原声乐器的现场录音。拍手和响指改用独立 CC0 实录，见 [BodyPercussion](../BodyPercussion/README.md)。

| 稳定编号 | 乐器 | bank 文件 | 上游 bank / program（从零计数） | 持续方式 |
| --- | --- | --- | --- | --- |
| 0 | 钢琴 | piano.bank | 0 / 0 | 自然衰减 |
| 1 | 电钢琴 | electric_piano.bank | 0 / 4 | 自然衰减 |
| 2 | 指弹贝斯 | bass.bank | 0 / 33 | 自然衰减 |
| 3 | 尼龙吉他 | guitar.bank | 0 / 24 | 自然衰减 |
| 4 | 弦乐 | strings.bank | 0 / 48 | 交叉循环 |
| 5 | 管风琴 | organ.bank | 0 / 19 | 交叉循环 |
| 6 | 长笛 | flute.bank | 0 / 73 | 交叉循环 |
| 7 | 电子主音 | lead.bank | 0 / 80 | 交叉循环 |
| 8 | 圆号 | horn.bank | 0 / 60 | 交叉循环 |
| 9 | 铜管 | brass.bank | 0 / 61 | 交叉循环 |
| 10 | 定音鼓 | timpani.bank | 0 / 47 | 自然衰减 |
| 11 | 人声元音 | voice.bank | 0 / 53 | 交叉循环 |
| 12 | 合唱 | choir.bank | 0 / 52 | 交叉循环 |

每个 bank 为 3,744,028 字节、13 个音区（MIDI 36–84、间隔 4 半音）、每区 6 秒、24 kHz / 单声道 / 16 位小端；13 个文件共约 48.7 MB，解码约 97.3 MB 内存。使用固定力度 100，关闭混响/合唱效果，每乐器全音区统一峰值归一。非循环音色在采样末尾淡出，持续音色保留起音后在 1–5 秒内做 100 ms 交叉循环。`MSCHOIR1` 是沿用的固定 bank 格式标识，并不限制内容只能是合唱。

七种鼓声从 bank 128 / program 0 离线渲染，MIDI 键分别为 36、38、42、46、45、50、49，对应 `kick.wav`、`snare.wav`、`closed_hat.wav`、`open_hat.wav`、`low_tom.wav`、`high_tom.wav`、`crash.wav`，最终为 48 kHz / 单声道 / 16 位 PCM。按实际资源长度保留尾音，吊镲最长 3.5 秒。

完整 `GeneralUser-GS.sf2` 仅在离线准备时使用，32,319,396 字节，被本目录 `.gitignore` 忽略；SHA-256 为 `9575028c7a1f589f5770fccc8cff2734566af40cd26ed836944e9a5152688cfe`。应用运行不读取 SF2，不链接 FluidSynth，也不提供通用 SoundFont 导入。

七种鼓声 WAV 和 13 个 `.bank` 均随仓库提供，正常构建无需重制。只重制鼓声时，可在下方命令末尾加 `--drums-only`。

重制时从上述官方固定提交取得原 SF2，在仓库根运行（Python 3 + libfluidsynth）：

```bash
python3 project_Demo/music_studio/tools/render_samples.py \
  extern_lib/GeneralUser-GS/GeneralUser-GS.sf2 extern_lib/GeneralUser-GS
```

找不到共享库时用 `--library /绝对路径/libfluidsynth.so`；只重制鼓声可加 `--drums-only`。生成器在录制鼓声前关闭旋律通道，避免其他乐器尾音混入。`samples.sha256` 固定运行资源校验值，CMake 验证清单；修改或重制后复核资源及清单再提交。资源缺失或格式不符明确失败，不回退到合成音。

旧工程中的 0–12 保留乐器身份但切换为采样声音；上一版重复的 13–16 / 17–20 读取时映射为钢琴 0、弦乐 4、铜管 9、长笛 6。工程不重复嵌入内置采样，依赖应用附带资源；用户导入的音频和鼓声替换采样仍随工程保存。
