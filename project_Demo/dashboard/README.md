# dashboard —— 实时监控面板(第七个 project_Demo)

用一个真实的系统/传感器监控面板,把此前只有孤立 demo、从没进过真应用的一簇控件
**一次性压出来**,并第一次在真应用里检验**数据绑定架构(UI=f(state))**。
本项目**不催生新控件**——纯用现成能力。

## 覆盖的控件 / 能力

| 控件 / 能力 | 在面板里的角色 | 此前状态 |
|---|---|---|
| Tabview | 总览 / 曲线 / 设置 三页 | 只有 demo_tabview |
| Meter | CPU、温度仪表盘 | 只有 demo_dashboard |
| ArcWidget | 内存环形进度 | 只有 demo_dashboard |
| Bar | 磁盘占用条 | 只有 demo/test |
| Chart | CPU/内存趋势折线 | 只有 demo_chart |
| Spinner | 顶部刷新指示 | 只有 demo_dashboard |
| Switch / Checkbox | 设置页开关项 | 只有 demo_form |
| Slider / Dropdown / Label | 间隔调节 / 采样源 / 读数 | 已验证,顺带用 |
| **数据绑定线** | 见下 | **从没进过任何 project_Demo** |

## 数据绑定架构(本项目重点)

后端只改状态,界面自动跟,全程不碰控件:

- **单向扇出**:`Meter_Bind / Arc_Bind / Bar_Bind / Label_Bind` 绑到 `cpu/mem/temp/disk`
  subject。后端 `State_SetInt` 推进 → 表盘/进度/读数全自动跟。
- **Chart 走 AddObserver**:Chart 是流式多序列不可 `*_Bind`,app 侧 `State_AddObserver`
  订阅 subject,值变时 `Chart_SetNext` 滚动。这是"控件绑定 + 自定义逻辑并存"的正解。
- **双向绑定**:设置页 `Switch_Bind(auto) / Checkbox_Bind(alarm) / Slider_Bind(interval)`,
  用户拨动直接回写 state,后端读 state 决定刷新节拍。
- **假传感器**:app 侧用库内 Q15 `GY_Sin` 合成三路相位错开的波形(纯整数、无 libm、
  headless 可复现),映射到 0..100。

## 分层铁律

控件不认识 "CPU/温度";传感器语义、合成波形、告警阈值全在本 app 侧。
GB2312 全字库外部 blob 回退(同 txt_edit/excel_edit,靠 `GB2312_BIN_PATH`)。

## 构建 / 运行

```sh
cmake -S project_Demo/dashboard -B build/project_Demo/dashboard
cmake --build build/project_Demo/dashboard
# 交互(SDL 窗口):
build/project_Demo/dashboard/dashboard
# headless 自检(argv[1]=帧数上限):
SDL_VIDEODRIVER=dummy build/project_Demo/dashboard/dashboard 30
```

headless 通过打印 `selftest: dashboard bind + chart feed OK` + `dashboard exit ok`,
退出码 0;selftest 失败退出码非 0。
