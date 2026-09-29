# Yaomi APP 模块接入

这是 phone_shell 内部的静态应用框架，不是 YMGUI 核心 API，也不是 Android/OS 插件系统。16 个业务应用各自独立编译、私有保存状态，共用桌面和宿主服务；320×480 三页外观不因本次拆分改变。

## 分层

```text
phone_shell.c                 桌面、导航、后台快照、开关机与宿主服务实现
phone_app_catalog.def         唯一注册清单，同时用于生成枚举/表和 CMake 源文件列表
phone_app.c / phone_app.h     描述符、查找、前后台生命周期、命令/查询派发
apps/*.c                     每个 APP 的私有状态、界面、回调和描述符
phone_ui.c / phone_ui.h       公共控件皮肤、文字/图标及现有 ANIM 的调用封装
phone_launcher.c / .h         桌面自动格子排版与分页
phone_desktop.c / .h          桌面顺序模型适配、长按菜单与跨页拖拽
phone_host.h                 APP 请求跳转、关闭、提示、关机和系统外观的服务
phone_ime.c / phone_locale.c  独立输入服务、中文字体生命周期
phone_shade.c / .h            下拉通知、快捷设置、手势与内存截屏预览
phone_quick*.c / .h / .def    快捷功能描述符、注册顺序、自动网格与内置演示状态
phone_selftest.inc / tests/   界面输入/像素回归与独立注册表生命周期测试
```

APP 不包含其他 APP 的 `.c`，不导出业务状态，不引用桌面的 `scene`、`running`、`app_views` 或其他 APP 的输入控件。公共绘图也不依赖 APP 注册表；图标颜色由调用方传入。

## 添加一个 APP

1. 将 [example.c.in](apps/example.c.in) 复制为 `apps/example.c`，实现界面和 `const PhoneApp PhoneApp_example`。名称、唯一 key、颜色、图标及生命周期都写在该文件中。
2. 在 [phone_app_catalog.def](phone_app_catalog.def) 追加一行：

```c
PHONE_APP(EXAMPLE, example)
```

3. 重新构建。CMake 从同一个清单自动加入源文件；不需要再修改 CMake、桌面数组、枚举、创建分支或预览参数。`--example` 自动成为预览入口；图标和名称自动排位，启动/返回、后台缩略图和清理复用现有流程。

清单的第二个参数对应文件名和描述符后缀，使用小写字母/数字/下划线；key 必须唯一，建议与文件名一致。内建图标从 `phone_ui.h` 选用；绘制全新图标时还需扩展公共图标库，这不是运行时图标资源加载器。

正式清单仍为 16 项。模板不加入默认构建，不增加演示桌面入口；额外第 17 项已经在临时构建中验证自动排位、打开/返回及后台操作。

## 生命周期契约

| 回调 | 时机与职责 |
| --- | --- |
| `create(view)` | 启动时为每个注册 APP 调用一次。只在给定容器内创建控件、初始化模块状态；此时导航/输入服务/提示层可能尚未创建，不调用跳转或提示服务。 |
| `show(argument)` | 打开/恢复到前台，包括同 APP 再次收到打开请求。参数为可选字符串，仅在调用期间有效，需要保留时拷贝；可为输入框带入号码等。 |
| `hide()` | 回桌面、进后台、切换应用时离开前台。不销毁会话，不等于后台清理。 |
| `tick(elapsed)` | 仅运行中的 APP 接收，后台也继续接收；时间片最大 100ms，与原演示一致。计时业务在这里处理，UI 补间仍由 ANIM 处理。 |
| `back()` | 输入法/系统弹窗处理之后，前台 APP 可消费返回；返回非零表示已处理，否则宿主回桌面。文件管理器借此返回父目录。 |
| `close()` | 单项关闭、全部清理或关机时停止活动；同一次关闭只调用一次。停止秒表、音乐或模拟录音，不清空会话笔记/设置。 |
| `destroy()` | 进程退出时，先 close，再销毁。释放 APP 自有的非控件资源、清空句柄；随后宿主递归释放 view。不要再次 Free view 或由 view 拥有的子控件。 |

除 `create` 外均可留空。所有回调在 UI 线程同步执行；不要阻塞，不要从生命周期回调重入导航/注册表派发。独立按键事件可请求宿主跳转。App 根容器只由宿主销毁。

当前是单 context、每 APP 单实例、启动时创建全部视图，不提供懒加载、动态安装/卸载、独立进程、跨线程访问或持久化。关闭 APP 后仍保留控件/会话状态，后台快照单独释放；退出才销毁控件树。内建应用之间仍有明确的 ID 依赖（例如通讯录依赖电话/短信），不能把任意删注册项理解为自动处理所有功能依赖。

## APP 间通信与宿主服务

- `PhoneHost_Open(PHONE, number)`：统一打开并把号码交给电话模块的 `show`。返回 0 表示当前忙碌/关机或 ID 无效，未打开；返回 1 表示已接受。通讯录不再直接访问电话或短信的输入框。
- `PhoneApps_Command(MUSIC, "toggle", NULL)`：桌面播放器调用公开命令，不访问音乐内部按钮；宿主先标记应用运行。命令本身不自动打开或激活应用。
- `PhoneApps_Query(RECORDER, "record-count")`：文件管理器取得演示记录数，不访问录音机变量。未知命令/查询或无效 ID 安全返回 0。
- `PhoneHost_Close()`：关闭前台 APP、返回桌面、移除运行标记和快照。
- `PhoneHost_Notice(title, message)`：复用置于导航和键盘之上的系统提示框，先收起输入法；支持关闭提示或退出 APP。
- `PhoneHost_SetTheme/SetBrightness`、`PhoneHost_Power`、`PhoneHost_MusicChanged`：设置/音乐对系统外观的显式服务接口，不传递内部控件。
- `PhoneHost_SetWifi/GetWifi/GetBrightness`：共享演示 Wi-Fi 状态和预览亮度；宿主同步控制中心与设置页，`settings` 的 `sync` 命令刷新其私有控件，不配置真实网络。
- `PhoneHost_Capture(pixels)`：UI 线程同步刷新并复制当前未调暗画面到调用者提供的 `320×480` native 像素缓冲；控制中心先关闭自身再调用，缓冲所有权仍归调用者，不落盘。不是平台截屏 API。

包含 `phone_ui.h` 后，新建 TextInput/EditView 默认经统一输入服务入口，保留原生编辑能力。`YAOMI_IME=OFF` 只裁输入服务和词典；`YMGUI_ANIM=OFF` 只将过渡改成即时设置，APP 生命周期与交互不变。

`inspect` / `PhoneApps_Inspect` 是保留原有自检断言的只读诊断入口，可返回控件句柄或数值；生产业务不得用它绕过模块边界。新增普通 APP 不要求提供该回调。

## 排版与验证

桌面第一页 4 项，其后每页 8 项、每行 4 项，默认按注册顺序自动排版。16 项为三页，超过 20 项自动扩页。现在支持长按后网格插入排序、边缘停留跨页、取消恢复；顺序模型与 APP ID 分离，启动位置跟随新顺序。长按菜单“卸载”只是可恢复的会话入口移除，宿主拒绝打开已移除 ID，不销毁模块代码/控件。APP 内部仍是固定坐标布局，无桌面持久化或任意像素布局。

快捷设置用另一套 [独立注册表](QUICK_GUIDE.md)，不与 APP 注册表或桌面手动顺序混用。

```sh
cmake --build build/rgb565/project_Demo/phone_shell
ctest --test-dir build/rgb565/project_Demo/phone_shell --output-on-failure
```

独立工程有两个测试：`phone_shell_ui` 覆盖真实输入、16 个入口、跨 APP 跳转、输入法、后台及逐帧局部/全量像素一致性；`phone_app_registry` 用独立测试清单验证 create/show/hide/tick/back/close/destroy、参数传递、后台继续运行、重复关闭/销毁、状态保留、无效 ID/命令，以及宿主管理的对象释放。根工程测试数量不变。
