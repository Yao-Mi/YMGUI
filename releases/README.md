# YMGUI 预编译库包

这里的分发包随 Git 仓库提交，拉取后可直接解压使用。当前提供 Linux x86_64、Release/PIC、RGB565 / RGB888 核心与 SDL 适配静态库，附头文件、CMake 接口、示例、字模、手册和验证日志。其他平台需从源码重新构建。

- [库包](YMGUI_libs-linux-x86_64.tar.gz)
- [SHA-256 校验文件](YMGUI_libs-linux-x86_64.tar.gz.sha256)

从仓库根目录执行：

```bash
(cd releases && sha256sum -c YMGUI_libs-linux-x86_64.tar.gz.sha256)
mkdir -p build/sdk-unpacked
tar -xzf releases/YMGUI_libs-linux-x86_64.tar.gz -C build/sdk-unpacked
cd build/sdk-unpacked/YMGUI_libs
sha256sum -c checksums.sha256
./build_demos.sh 16
```

接入项目时，将 `YMGUI_DIR` 指向解压目录内的 `cmake/`。SDL 示例需要系统 SDL2 开发包；核心使用不依赖 SDL。详见 [SDK 说明](../sdk/README.md) 和 [接入手册](../sdk/MANUAL.md)。

维护者用 `./sdk/build.sh --release` 重新构建并验证分发包；可追加 `--ymgre-dir /path/to/YMGRE_libs/cmake` 验证 YMGRE 联用，然后将新压缩包、校验文件及相关源码和文档一起提交。普通 `./sdk/build.sh` 仅生成被 Git 忽略的本地 `build/` 产物。
