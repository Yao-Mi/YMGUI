# 拍手与响指实录采样

供 `project_Demo/music_studio` 使用，两种音色都播放 PCM，不再使用合成响指。资源按 CC0 1.0 分发，许可全文见 [LICENSE.txt](LICENSE.txt)；下列作者没有为本应用的裁剪、均衡或播放效果背书。

| 素材 | 原作者 / 来源 | 固定版本与原文件 SHA-256 |
| --- | --- | --- |
| `clap_source.wav` | VCSL / Versilian Studios，[SoloClap_vl3.wav](https://github.com/sgossner/VCSL/blob/c1ea7bcc3c7309650ab0da9d15c9cd1fbc4a4c7e/Idiophones/Struck%20Idiophones/Claps/SoloClap_vl3.wav) | 提交 `c1ea7bcc3c7309650ab0da9d15c9cd1fbc4a4c7e`；`03dc4fa78deabcbfa0972accf45e538b25b69526e69fad6a81b48ca508caeb63` |
| `snap_source.wav` | Vinrax，[Snap of the fingers sounds](https://lpc.opengameart.org/content/snap-of-the-fingers-sounds)，`snap_of_the_fingers3.wav` | 原作者页面列 CC0；`bbcae48d531e02060a6ed6efec7bdf12f606fb32bfb44567f1e1463fc767a05a` |

本目录 WAV 文件随仓库提供，包含原始录音和处理结果，正常构建无需重制。原始录音便于复现；重制脚本按上表 SHA-256 验证来源。`clap.wav` 与 `snap.wav` 是应用使用的 48 kHz / 单声道 / 16 位版本，分别约 163 ms 和 96 ms。处理包括 650 Hz 二阶高通、2.6 kHz 高频搁架 +6 dB、清除起音前空白、保留 0.5 ms 预卷、仅两采样点起音衔接、末尾 5 ms 淡出、峰值归一至 0.75。没有用噪声或振荡器替换原录音。

在仓库根重制（Python 3 + FFmpeg，仅开发准备时需要）：

```bash
python3 project_Demo/music_studio/tools/prepare_body_percussion.py extern_lib/BodyPercussion
```

`samples.sha256` 是运行资源的构建校验清单。脚本先验证原文件校验值，再生成结果和清单；应用初始化验证 WAV 格式及长度。界面“内置鼓声”会恢复处理后的实录素材，“更换鼓声”仍可加载用户自己的采样。
