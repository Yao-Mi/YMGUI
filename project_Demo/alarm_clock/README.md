# alarm_clock —— 闹钟(第八个 project_Demo)

用一个真实的闹钟,把刚落地的 **MsgBox 模态弹窗**放进真应用里压一遍:
到点 + 使能开 → 弹出**阻塞模态**,半透明遮罩锁住整个界面,必须点"关闭"按钮才解锁。
本项目**不催生新控件**——纯用现成能力。

## 覆盖的控件 / 能力

| 控件 / 能力 | 在闹钟里的角色 | 此前状态 |
|---|---|---|
| **MsgBox** | 到点弹出的阻塞模态,遮罩锁底层 | **刚落地,首次进真应用** |
| Roller ×3 | 时 / 分 / 秒 设定闹钟时刻 | 只有 demo_roller |
| Switch | 闹钟使能开关 | 已验证,顺带用 |
| Label / Button | 当前时间大字 / 时刻标签 / 状态行 / 模态按钮 | 已验证 |

## 模态弹窗(本项目重点)

"阻塞"在单事件循环 + retained-mode 架构下,不是线程阻塞,而是**输入模态**:

- MsgBox 是库控件,句柄=一块**全屏遮罩**,挂在 `top_layer`(root 的兄弟,永远画在最上、
  命中测试最先)。遮罩画半透明黑压暗底层,并**吞掉卡片以外的所有点击** → 底层 UI 锁死。
- 卡片(遮罩的子)上放标题 / 多行正文 / 按钮。只有点卡片按钮才 `Hide` 模态解锁。
- app 侧只管配置文案 + `Show`;关闭按钮回调回写状态标签。这正是 LVGL `lv_msgbox` 的异步回调风格。

## 时钟模型(app 侧)

- `g_now_sec` = 当天已过秒数(0..86399),每帧 +1(演示可见跳秒),到 86400 归 0。
- 到点判定:`now==闹钟秒 && 使能开 && 本次未触发`(`g_fired` 去抖)→ `MsgBox_Show`。
  过了这一秒清 `g_fired`,下一整天同一时刻能再响。
- 闹钟时刻由三个 Roller 的选中项算出:`(h*60+m)*60+s`。

## 分层铁律

控件不认识"闹钟/时刻";时间推进、到点判定、去抖全在本 app 侧。
GB2312 全字库外部 blob 回退(同 dashboard/txt_edit,靠 `GB2312_BIN_PATH`)。

## 构建 / 运行

```sh
cmake -S project_Demo/alarm_clock -B build/rgb565/project_Demo/alarm_clock
cmake --build build/rgb565/project_Demo/alarm_clock
# 交互(SDL 窗口):
build/rgb565/project_Demo/alarm_clock/alarm_clock
# headless 自检(argv[1]=帧数上限):
SDL_VIDEODRIVER=dummy build/rgb565/project_Demo/alarm_clock/alarm_clock 20
```

headless 把闹钟设在"当前+2秒",跑几帧应触发模态,再模拟点关闭 → 模态隐藏、遮罩解锁。
通过打印 `selftest: alarm modal + roller OK` + `alarm_clock exit ok`,退出码 0;失败退出码非 0。
