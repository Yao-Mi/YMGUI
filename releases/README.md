# YMGUI 预编译库包

这里的分发包随 Git 仓库提交，拉取后可直接解压使用。当前提供 Linux x86_64、Release/PIC、RGB565 / RGB888 核心与 SDL 适配静态库，附头文件、CMake 接口、示例、字模、手册和验证日志。其他平台需从源码重新构建。

- [库包](YMGUI_libs-linux-x86_64.tar.gz)
- [SHA-256 校验文件](YMGUI_libs-linux-x86_64.tar.gz.sha256)

## 当前版本

2026-10-08 更新，库源码基线为 `c08fef4`，同时包含本次 SDK 配置与验证工具更新；精确源码摘要及构建状态见包内 `BUILD_INFO.json`。包括回流后的渲染与脏区修复、Arena、FontCache、Resource、Bus、StagedFlush、TextActions、完整 Touch 接口，以及 SDL 多点适配。提供 RGB565 / RGB888 核心和 SDL 静态库及匹配头文件。

固定配置为 `YMGUI_ANIM=1`、16位坐标、`GY_INV_MAX=16`、`GY_TOUCH_MAX=5`、`YMGUI_OCCLUSION=0`、`YMGUI_CLIPBOARD_DYNAMIC=0`、`YMGUI_RESOURCE_STDIO=1`。可选遮挡/动态剪贴板沿用通用库默认关闭配置；更改功能配置或平台时从源码重建，不能只改消费端宏。

已验证仓库外两种色深的纯核心各1项、SDL各2项示例和错误配置拒绝；实际压缩包解压后的每种色深均通过动画、移植优化、资源支持和SDL输入四项回归。压缩包与包内文件的SHA-256校验通过；本轮没有重跑外部YMGRE示例。此包发布库和SDK示例，Yaomi Phone可移植应用仍使用仓库源码。

从仓库根目录执行：

```bash
(cd releases && sha256sum -c YMGUI_libs-linux-x86_64.tar.gz.sha256)
mkdir -p build/checks/sdk-unpacked
tar -xzf releases/YMGUI_libs-linux-x86_64.tar.gz -C build/checks/sdk-unpacked
cd build/checks/sdk-unpacked/YMGUI_libs
sha256sum -c checksums.sha256
./build_demos.sh 16
```

接入项目时，将 `YMGUI_DIR` 指向解压目录内的 `cmake/`。SDL 示例需要系统 SDL2 开发包；核心使用不依赖 SDL。详见 [SDK 说明](../sdk/README.md) 和 [接入手册](../sdk/MANUAL.md)。

维护者用 `./sdk/build.sh --release` 重新构建并验证分发包；可追加 `--ymgre-dir /path/to/YMGRE_libs/cmake` 验证 YMGRE 联用，然后将新压缩包、校验文件及相关源码和文档一起提交。普通 `./sdk/build.sh` 仅生成被 Git 忽略的本地 `build/` 产物。

SDK 成品和中间文件分别集中于 `build/sdk/YMGUI_libs/`、`build/sdk/work/`，日志位于 `build/logs/sdk/`。维护者也可直接运行 `./sdk/verify.sh` 复验已生成的压缩包；临时解压/编译文件自动清理，不会在 build 顶层留下散落产物。
