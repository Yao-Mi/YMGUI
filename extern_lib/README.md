# project_Demo 第三方依赖

`extern_lib/` 存放供 `project_Demo/` 应用按需使用的第三方库源码。各应用通过自己的构建配置引用所需依赖，编译产物放入对应应用的 `build/` 目录，保留第三方源码及许可证。

| 目录 | 使用方 | 用途 |
| --- | --- | --- |
| `FFmpeg/` | `project_Demo/video_stidio/` | 视频信息读取、预览与导出源帧解码 |

应用的 GUI 直接使用仓库内的 `YMGUI/` 和 `SDL_LCD/`。`extern_lib/` 不属于 YMGUI 核心库的移植范围，移植核心库时无需一并复制。

FFmpeg 的构建选项和命令行依赖见 [video_stidio 说明](../project_Demo/video_stidio/README.md)。
