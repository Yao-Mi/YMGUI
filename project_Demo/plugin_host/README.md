# Plugin Manager

完整的跨平台插件管理器验证项目，800×480：

- 启动时扫描管理器可执行文件所在目录，左侧只显示通过 ABI 校验的插件；
- 顶部提供加载、卸载操作和运行摘要；
- 下方记录插件生命周期与操作日志；
- `system_info` 动态插件自行创建右侧内容页、指标卡和刷新按钮；
- 卸载时销毁插件 UI 和插件内按钮回调，再关闭动态库；
- Linux 生成 `system_info.so`，Windows 生成 `system_info.dll`，Android 生成
  `libsystem_info.so`；裸机继续使用静态注册。

Linux 构建运行：

```bash
cmake -S project_Demo/plugin_host -B build/rgb565/project_Demo/plugin_host
cmake --build build/rgb565/project_Demo/plugin_host
./build/rgb565/project_Demo/plugin_host/plugin_host
ctest --test-dir build/rgb565/project_Demo/plugin_host --output-on-failure
```

CTest 会在 SDL dummy 驱动下自动执行一次卸载、重新加载和插件内刷新操作。
测试目录还会生成一个普通动态库 `not_a_plugin`，用于确认没有
`YMGUI_Plugin_Get` 和合法描述符的 `.so/.dll` 不会出现在侧边栏；另一个用例会让插件
保持加载并直接关闭宿主，验证退出路径不会卡顿；空目录用例验证删除插件后列表为空。

也可用第二个参数指定扫描目录：

```bash
./build/rgb565/project_Demo/plugin_host/plugin_host -1 /path/to/plugins
```

探测会打开候选动态库来读取导出描述符，因此能过滤不兼容库，但不构成安全沙箱；
只应扫描和加载可信来源的本机代码。

Android 插件必须随 APK/AAB 按 ABI 打包到 native library 目录。独立 CMake 构建需传
`SDL2_SOURCE_DIR`；由 Gradle 或父工程预先提供 `SDL2::SDL2` target 时无需重复传入。
