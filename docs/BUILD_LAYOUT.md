# 构建产物目录

源码留在 `Demo/`、`project_Demo/` 和 `YMGUI/`；默认构建产物全部进入被 Git 忽略的 `build/`，按色深和类型分组。可分发库包单独提交到 `releases/`。

```text
build/
  rgb565/
    Demo/                          根工程的统一构建目录
      demo_*                       28 个控件 / 机制演示
      test_*                       35 个核心测试
      libymgui.a                   核心库
      CMakeFiles/                  编译中间文件
    project_Demo/
      alarm_clock/alarm_clock      应用与其资源、库、中间文件在各自目录
      chinese_ime/chinese_ime
      video_stidio/video_stidio
      ...                          共 13 个独立应用
  rgb888/
    Demo/                          RGB888 的同一套 Demo / 测试 / 核心库
    project_Demo/                  12 个支持 RGB888 的应用
  depth1/Demo/                     可选 1bpp 验证
  depth8/Demo/                     可选 8bpp 验证
  _ymgui_sdk/                      SDK 构建中间文件
  YMGUI_libs/                      本地完整 SDK
releases/
  YMGUI_libs-linux-x86_64.tar.gz    已入库的分发包
  YMGUI_libs-linux-x86_64.tar.gz.sha256
```

`Demo/` 的演示共用根 CMake，因而所有演示、根测试和核心库在一个构建目录。`project_Demo/` 中每个应用是独立 CMake 工程，各自保留完整构建目录；它们归在同一父目录下，不把不同应用的资源、插件或依赖库混放。视频剪辑器仅支持 RGB565。

## 常用命令

以下命令从仓库根运行，脚本自身也支持从其他目录调用。

```bash
./build_all.sh -t                   # RGB565 Demo + 12 应用，运行根工程 35 项测试
./build_all.sh --depth 24 -t        # RGB888 Demo + 11 应用，跳过 video_stidio
tools/test.sh --depth 16            # 只构建并测试根工程，复用 rgb565/Demo
tools/test_matrix.sh                # 仓库检查与两种色深的根工程测试
./build/rgb565/Demo/demo_button
./build/rgb565/project_Demo/alarm_clock/alarm_clock
```

只构建一个控件演示：

```bash
cmake -S . -B build/rgb565/Demo -DYMGUI_COLOR_DEPTH=16
cmake --build build/rgb565/Demo --target demo_button -j8
```

只构建一个完整应用：

```bash
cmake -S project_Demo/alarm_clock -B build/rgb565/project_Demo/alarm_clock -DYMGUI_COLOR_DEPTH=16
cmake --build build/rgb565/project_Demo/alarm_clock -j8
```

`build_all.sh` 默认构建并行数不超过 8，可用 `YMGUI_BUILD_JOBS` 调整；根测试脚本对应 `YMGUI_TEST_JOBS`。`build_all.sh -t` 只运行根测试，应用自带的 CTest 仍应在各自目录执行。

## 截图与分发

```bash
./capture_shots.sh -b                              # 构建 RGB565 后输出 docs/shots
./capture_shots.sh --output build/shots-check       # 验证截图流程，不更新文档图片
./capture_shots.sh --depth 24 --output build/shots-rgb888
./sdk/build.sh                                    # 本地 SDK 与压缩包
./sdk/build.sh --release                          # 验收后更新 releases 分发包
```

截图脚本只从当前色深的统一目录读取产物；缺少二进制、运行失败或超时均报错。视频剪辑器使用 `--frames` 有限帧启动，其批量截图是空项目界面；README 的真实素材时间线截图仍按应用说明单独生成。

## 清理与旧目录

`./build_all.sh -c` 只删除并重建选中色深的 `Demo/` 以及当前源码中各完整应用的构建目录，不清空整个 `build/`，也不删除 SDK、分发包或另一种色深。

历史上直接用 `-B build`、`-B build/rgb565` 或测试默认路径 `build/verify/rgb16` 生成的旧产物可能仍在本机。新脚本不再读取它们，也不自动删除旧目录中的个人文件。CMake 缓存包含绝对路径，不能把旧构建目录直接移动成新目录；使用上述入口重新配置 / 构建即可。手动指定 `cmake -B` 或 `tools/test.sh --build-dir` 时，产物仍落在指定位置。
