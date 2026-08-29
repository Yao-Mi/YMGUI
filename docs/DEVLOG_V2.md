# YMGUI 开发进程文档 V2

V2 是 [V1](history/DEVLOG_V1.md) 的直接续集，从第 42 轮开始持续记录开发过程、技术取舍、问题和验证结果。本文与 V1 保持相同定位，是项目的连续“来时路”，不是压缩版 Changelog。

当前事实仍以 [当前状态](当前状态.md)、代码、测试和 CMake 为准；V2 记录一项工作是如何演进到该状态的。V1 已归档，除追溯旧决策或排查回归外不需默认读取。

---

## 第 42 轮：RGB888 framebuffer 与双格式构建

- 新增 `YMGUI_COLOR_DEPTH=24`：`GYpx` 为 packed 的 3 字节 RGB888（内存顺序 R、G、B），补齐颜色转换、alpha 混合、图片 colorkey、SDL `RGB24` 输出。
- 保留并验证 `YMGUI_COLOR_DEPTH=16` 的 RGB565 路径；像素比较统一使用 `GY_PxEqual`，清零使用 `GY_PX_ZERO`，避免把 RGB888 结构体当整数访问。
- 顶层工程和 `project_Demo` 共用格式选项，构建目录按格式归档：`build/rgb565/...`、`build/rgb888/...`。
- `video_player` 按格式选择 FFmpeg `rgb565le` 或 `rgb24` 原始帧输入。
- 验证结果：顶层 RGB565/RGB888 各 35 项测试全部通过；10 个 `project_Demo` 在两种格式下均构建通过。
- **本轮补充：窗口关闭请求回调**。`SDL_LCD_SetCloseRequestCb(cb, user)` 仅属于桌面 SDL port；收到 `SDL_QUIT` 时，回调返回 0 取消本次关闭，返回非 0 才使 `SDL_LCD_PumpEvents()` 返回 0。未注册时保持旧行为，`Esc` 快捷退出不经过该回调。新增 `demo_close_request`（第一次关闭取消，第二次允许）和 `test_sdl_context` 允许/取消回归覆盖。

---

## 第 43 轮：GB2312 字模入库 + 文档分层与换卷

**GB2312 字模入库**：用户反馈不知道如何生成 `gb2312_glyphs.bin`，进一步希望直接下载该文件。原仓库在 `.gitignore` 中排除了 blob，只保留生成脚本和码点索引。现移除该排除规则，将 `Demo/gb2312_glyphs.bin` 正式纳入版本管理，普通使用者不再需要先安装 Pillow/Noto CJK 再生成。生成能力仍保留，用于修改字体、字集或光栅化参数。

**字库现状核对**：重跑 `Demo/gen_font.py --cjk --gb2312 --extern --bin` 并与入库 blob 逐字节比较。当前字集不是旧文档所说的 6763 个纯汉字，而是 GB2312 符号/标点区、一二级汉字及额外 `—–·` 共 7448 字形。16×16、4bpp，每字 128 字节，blob 为 953344 字节，SHA-256 为 `c2c42b2c1c2041c158a13b162549a03504202887d8179f476f9ef1a152d51316`。生成脚本帮助、配置头和 Demo 注释中的旧数字同步修正。

**文档分层**：根目录只保留面向使用者的 `README.md` 和维护规则 `AGENTS.md`。API、架构、跨平台移植和代码风格迁入 `docs/`；原 `PROJECT.md` 作为旧项目快照迁入 `docs/history/`；新建 `docs/当前状态.md` 作为当前事实的权威入口。维护者默认先读当前状态，再按任务读专题文档，不因为日常工作重读整卷旧日志。

**当前状态汇总**：用户进一步明确 `当前状态.md` 应当是“精简所有 DEVLOG”，而不只是入口摘要。因此按当前实现重写为完整功能总表：汇总显示/渲染、对象树、输入、内存与裁减、图元/字体、27 个具名控件、状态绑定、插件、平台适配和 10 个完整应用，并单列当前边界。今后“现在有什么”只查当前状态，“当时为什么这样做”才查 DEVLOG。

**DEVLOG 换卷**：用户明确 DEVLOG 仍是连续的项目开发记录，不应被 Git 历史完全取代。因此将第 0～41 轮归档为 `docs/history/DEVLOG_V1.md`，并在 `docs/DEVLOG_V2.md` 从第 42 轮继续。V1 的价值是保留历史上下文，V2 的价值是承接后续过程；`当前状态.md` 则避免日常工作必须从长日志里推断现状。

**验证**：GB2312 blob 可重现生成且与入库文件完全一致；Markdown 本地链接检查通过；根工程完整构建通过，**ctest 35/35**；`git diff --check` 通过。

---

## 第 44 轮：工具目录归位与自动化验证入口

**生成工具和产物归位**：新增仓库级 `tools/`，将字符/字体生成器从 `Demo/gen_font.py` 迁到 `tools/gen_font.py`，将已入库的外部字模从 `Demo/gb2312_glyphs.bin` 迁到 `tools/gb2312_glyphs.bin`。`Demo/` 继续只承担示例程序，生成器、可再生产物和维护脚本归入工具目录；三角函数表生成器仍留在 `Demo/`，因为本轮只调整字符生成链路。顶层 CMake、8 个使用中文回退字库的完整应用、源码注释和当前文档中的资源路径全部同步更新。V1 及第 43 轮中的旧路径保留为当时事实。

**字库验证自动化**：新增 `tools/verify_gb2312.sh`。快速模式检查入库 blob 的字形数、字节数和 SHA-256；`--rebuild` 模式调用 Pillow 和 Noto Sans CJK，在临时目录重新生成 blob 与 `YMGUI_FontDataGB2312.c`，再分别逐字节比较。这样既能尽早发现资源损坏，也能验证生成器、字形顺序和索引没有漂移。

**仓库级自动化入口**：新增 `tools/check_docs.py` 检查 Markdown 本地链接；`tools/check_repo.sh` 汇总链接、blob、`git diff --check`、关键数量和过期路径检查；`tools/test.sh` 配置、构建并测试一个色深；`tools/test_matrix.sh` 默认覆盖 RGB565/RGB888，`--all-depths` 可覆盖 1/8/16/24bpp。构建并发默认封顶为 8，可通过 `YMGUI_TEST_JOBS` 覆盖，避免高核数但资源受限的环境拉起过多编译任务。用法集中写入 `tools/README.md`，根 README 只保留常用入口。

**验证**：Python 脚本编译检查与 Shell 语法检查通过；Markdown 32 个本地链接全部有效；仓库计数和旧路径检查通过；7448 字形、953344 字节的 GB2312 blob 重生成后与入库文件一致；RGB565、RGB888 根工程均完整构建，两个配置各 **ctest 35/35**；`SDL_VIDEODRIVER=dummy` 下 `demo_font_gb2312` 成功运行 5 帧并从新路径读取字模（40 次模拟 flash 读取）；`git diff --check` 通过。
