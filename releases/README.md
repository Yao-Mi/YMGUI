# YMGUI 预编译库包

这里的分发包随 Git 仓库提交，拉取后可直接解压使用。当前提供 Linux x86_64、Release/PIC、RGB565 / RGB888 核心与 SDL 适配静态库，附头文件、CMake 接口、示例、字模、手册和验证日志。其他平台需从源码重新构建。

- [库包](YMGUI_libs-linux-x86_64.tar.gz)
- [SHA-256 校验文件](YMGUI_libs-linux-x86_64.tar.gz.sha256)

## 当前版本

2026-09-29 清空构建目录后重新生成，源码基线为 `9700a66`，并包含本次 SDK 目录/脚本整理（与发布包一同提交，构建时标记为未提交修改，精确源码摘要见包内元数据）。默认包含 `YMGUI_ANIM=1` 动效模块、匹配头文件及 RGB565 / RGB888 的核心和 SDL 适配库。预编译包的动效开关属于固定 ABI，应用侧不能单独关闭；需要裁剪时从源码重新构建。

已验证仓库外核心/SDL 示例、配置不匹配拒绝、YMGRE index16/index32 各 30 项测试，以及解压后两种色深的动效核心回归。压缩包和包内文件均通过 SHA-256 校验；确切来源、构建配置与日志见包内 `BUILD_INFO.json`、`verification/`。此包只发布库和 SDK 示例，不包含 Yaomi Phone 等完整应用。

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
