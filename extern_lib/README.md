# project_Demo 第三方依赖

`extern_lib/` 存放供 `project_Demo/` 应用按需使用的第三方库源码和音源数据。各应用通过自己的构建配置引用所需依赖，编译产物放入对应应用的 `build/` 目录，保留第三方源码及许可证。

| 目录 | 使用方 | 用途 |
| --- | --- | --- |
| `BodyPercussion/` | `project_Demo/music_studio/` | CC0 实录拍手/响指、原始素材、许可证与处理脚本说明 |
| `GeneralUser-GS/` | `project_Demo/music_studio/` | 13 类乐器采样 bank、七种鼓声 WAV、上游许可与重制说明；完整 SF2 仅本地准备使用 |
| `FFmpeg/` | `project_Demo/video_stidio/` | 视频信息读取、预览与导出源帧解码 |

应用的 GUI 直接使用仓库内的 `YMGUI/` 和 `SDL_LCD/`。`extern_lib/` 不属于 YMGUI 核心库的移植范围，移植核心库时无需一并复制。

FFmpeg 的构建选项和命令行依赖见 [video_stidio 说明](../project_Demo/video_stidio/README.md)。

音乐工坊的乐器 `.bank`、鼓声 WAV 和拍手/响指原始录音均纳入 Git，正常构建无需重制。来源许可、校验清单及重制方法见 [GeneralUser-GS 说明](GeneralUser-GS/README.md) 和 [拍手/响指说明](BodyPercussion/README.md)。根 `.gitignore` 对 `extern_lib/` 的 WAV 保留例外，其他位置的 WAV 和本地 `参考/` 仍排除。
