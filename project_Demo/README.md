# project_Demo —— 基础验证项目

用**真实的小应用**跑通 YMGUI,验证功能是否够用、暴露该补的缺口。区别于 `Demo/`(孤立演示单个控件),这里每个子文件夹是一个**独立工程**:把仓库里的 `YMGUI/` 当外部依赖引入,自己 configure/build,项目间互不牵连。

## 约定

- 每个项目一个子文件夹,内含自己的 `CMakeLists.txt` 和源码。
- 各项目 CMakeLists 只需 include 共享咬合层 `../ymgui_app.cmake`,再一行 `ymgui_add_app(...)`。样板(10 层裸头 include、GLOB 库源码建 `ymgui`、SDL_LCD 建 `sdl_lcd`、找 SDL2)全在咬合层里,不必每项目重抄。
- 共享咬合层在 Linux 上通过 `pkg-config` 找 SDL2,在 Windows/Android 上使用 SDL2 CMake target；Android 应用目标构建为 shared library,Windows 可用 `-DYMGUI_SDL_STATIC=ON` 优先选择静态 SDL2 target。
- 编译输出按色深隔离：RGB565 使用 `build/rgb565/project_Demo/<项目名>/`，RGB888 使用 `build/rgb888/project_Demo/<项目名>/`；configure 时分别指定 `-DYMGUI_COLOR_DEPTH=16` 或 `24`。跨平台工具链和输入适配见 [`../docs/CROSS_PLATFORM_PORTING.md`](../docs/CROSS_PLATFORM_PORTING.md)。

`extern_lib/` 是 `project_Demo/` 的第三方依赖目录，各应用按需使用，例如 `video_stidio` 使用其中的 `FFmpeg/`。应用仍直接复用仓库内的 `YMGUI/` 和 `SDL_LCD/`；第三方库的编译产物放在对应应用的 build 目录。

单控件 `Demo/` 的产物集中在同色深的 `build/rgb565/Demo/` 或 `build/rgb888/Demo/`；这里的完整应用则各占 `project_Demo/<项目名>/` 子目录，避免可执行文件、资源和插件混在一起。`./build_all.sh` 一次构建 RGB565 全部应用，`--depth 24` 构建 RGB888 支持的应用（跳过 video_stidio）。完整布局见 [构建目录说明](../docs/BUILD_LAYOUT.md)。

## 加一个新项目

以下命令均从仓库根目录执行（即包含 `CMakeLists.txt`、`YMGUI/` 和 `project_Demo/` 的目录）。构建目录不是项目源码目录，而是根目录下对应色深前缀的目录。

RGB888 示例：

```bash
cmake -S project_Demo/my_app -B build/rgb888/project_Demo/my_app -DYMGUI_COLOR_DEPTH=24
cmake --build build/rgb888/project_Demo/my_app -j
```

```bash
mkdir -p project_Demo/my_app
# 写 project_Demo/my_app/CMakeLists.txt(照下面模板) + my_app.c
cmake -S project_Demo/my_app -B build/rgb565/project_Demo/my_app -DYMGUI_COLOR_DEPTH=16
cmake --build build/rgb565/project_Demo/my_app -j
./build/rgb565/project_Demo/my_app/my_app            # 有 SDL 显示时直接跑
SDL_VIDEODRIVER=dummy ./build/rgb565/project_Demo/my_app/my_app 30   # 无头跑 30 帧
```

`CMakeLists.txt` 模板:

```cmake
cmake_minimum_required(VERSION 3.16)
project(my_app C)
include(${CMAKE_CURRENT_LIST_DIR}/../ymgui_app.cmake)
ymgui_add_app(my_app my_app.c)
```

约定:`main(argc, argv)` 收一个可选 `argv[1]=帧数上限`,便于 headless 验证(跑够帧数就退出,退出前打印 `xxx exit ok`)。判定成败以命令 **exit code** 为准。

## 现有项目

- **music_studio（音乐工坊）** —— 中文多轨音乐制作工作台，鼓机、钢琴卷帘、音频编排、基础混音、保存和 WAV 导出；Linux / RGB565 / RGB888，带内置演示与独立测试（见 [说明](music_studio/README.md)）。

- **txt_edit** —— 行式文本编辑器。第一个验证项目,产出了第一份缺口清单(见 `txt_edit/README.md`)。
- **files_manager** —— 沙箱文件管理器。第二个验证项目,催生库控件 `TreeView`(见 `files_manager/README.md`)。
- **excel_edit** —— 电子表格。第三个验证项目,催生库控件 `Grid`,公式引擎在 app 侧(见 `excel_edit/README.md`)。
- **image_edit** —— 类 PS 图层画板。第四个验证项目,催生库控件 `Canvas` + `ColorPicker`,图层/融合/合成全在 app 侧(见 `image_edit/README.md`)。
- **music_player** —— 音乐播放器。第五个验证项目,催生库控件 `Roller`(居中高亮平滑滚动列表) + `BarChart`(通用柱状图,此处当频谱用);音乐语义(ffmpeg 解码 / SDL2 声卡 / .lrc 解析 / 频段分析)全在 app 侧(见 `music_player/README.md`)。
- **context_gesture** —— 上下文手势实验室。用基础对象、自定义绘制和 top_layer 菜单验证右键/触摸长按、捕获式拖动及取消恢复；不新增控件(见 `context_gesture/README.md`)。
- **chinese_ime** —— 中英文虚拟键盘。中文单字、中文词语和英文单词三份数据独立外挂，支持拼音/英文前缀候选、动态分页、点击或数字选择、用户频率学习和 CJK 字体回退。

- **video_stidio** —— Linux 视频剪辑器，素材导入、8 条视频轨、修剪/分割、撤销重做、项目保存和后台 H.264 导出；独立编译 FFmpeg，仅支持 RGB565，使用 `--frames N` 无头退出（见 [说明](video_stidio/README.md)）。
