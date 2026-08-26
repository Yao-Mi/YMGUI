# YMGUI 开发进程文档

按时间顺序记录每一轮做了什么、遇到什么问题、怎么解决的。开发始于 2026-07。

---

## 第 0 轮：架构讨论

在写代码前先敲定了地基决策（渲染模型/UI 范式/目标平台/依赖策略）。关键转折点是用户指出**裸机 LCD 只有画点和 blit 两个原语**，这直接倒转了分层——软件光栅化器成为核心而非"可选后端"，GPU 后端反而降级为可选。确立了"SDL 假装成一块 LCD"的开发模式。

产出：`代码风格.md`（借鉴 YMCV，前缀 CV→GY），目录命名对齐 YMCV。

---

## 第 1 轮：地基（CONFIG/DEBUG/HAL/CORE 骨架）

搭起最小可跑链路：三层颜色类型、双档内存、assert/log、HAL flush 契约、Surface 抽象、DrawFill、SDL 假 LCD、band 刷新 demo。

**问题 1**：SDL2 缺 `-dev` 包，pkg-config 找不到。sudo 需密码，我无法输入 → 让用户用 `! sudo apt-get install -y libsdl2-dev` 自行安装。在等待期间先写完所有不依赖 SDL 的代码。

**问题 2**：demo 用了 `gy_log_print` 却没 include debug 头 → 链接错误 `undefined reference`。加 include 解决。这是真实的编译错误，靠 exit code 和带行号的报错定位。

产出：`test_fill` 像素正确性单测通过。

---

## 第 2 轮：GUI 核心（对象树/失效/事件/Button）

做了保留模式的发动机：对象树(树级联 Free)、失效/脏矩形、Refresh 与 band 咬合、事件分发+命中测试、Button 控件。

**问题 3（自己的疏忽）**：我一度声称"build is clean"但其实还没编译新加的 GUI/OPOBJ 文件。立刻纠正、真正跑了 build 才确认。教训：不能凭记忆说通过，必须实跑。

**关键验证**：`test_event` 证明脏矩形真的省重绘——点击按钮后 flush 只覆盖 y=90..150（按钮所在），而非整屏 0..240。

---

## 第 3 轮：多矩形脏区 + 位图字体 + Label

- 脏区从单包围盒升级为多矩形列表（连通分量合并）。`test_invalidate` 验证两个远端脏区刷 ~800px 而非 76800px（96× 提升）。
- 位图字体子系统：`gen_font.py` 栅格 DejaVuSansMono → 8×16 点阵。
- Label 控件 + Button 加标题文字。

**过程判断**：我中途想直接开工中文字体，但意识到那是大特性（字库大、要 UTF-8 解码+稀疏查找+按需子集），该由用户决定，于是收住，先把控件+ASCII 字体做扎实。

---

## 第 4 轮：表单四件套（Checkbox/Switch/Slider/Bar）

先补了**指针 move 事件**(`GY_EVENT_Pressing`)——Slider 拖动的前提。ctx 存 point_x/y 供控件读。然后四个纯矩形控件。

**问题 4**：Slider 里把 `GYrect` 值 `abs` 误用成指针写了 `abs->h`（应为 `abs.h`）→ 真实编译错误 exit 2。修正后 `test_form` 验证拖动语义（按右→高值/拖左→0/拖中→~50）。

---

## 第 5 轮：图元层（定点三角/DrawLine/DrawImg/DrawArc）

补齐图元：Q15 定点三角表(`gen_trig.py`)、Bresenham 画线、图片 blit(colorkey)、中点画圆/圆弧。`test_draw` 全套像素验证一次通过。这一轮解锁了后续一大批显示控件。

## 第 6 轮：显示控件（Image/Arc/Spinner/Meter）

用第 5 轮的图元做了四个显示控件。Spinner 靠 `Tick` 由主循环驱动、无内部定时器（裸机友好）。Meter 用定点三角 `polar()` 算刻度/指针端点。

**问题 5（自己的测试逻辑错）**：`test_display` 里 Arc 那条第一次挂了(exit 1)。原因是我用"非零像素总数"判断前景弧随值增长，但背景弧总是画满、前景在**相同半径**上原地覆盖，所以总数几乎不变。修法：给前景独立颜色、专门统计前景色像素数。是测试写错，不是控件错。

---

## 第 7 轮：粗弧修复 + 键盘焦点 + TextInput

- **修环形进度黑洞**：用户反馈环上有未填充的黑点。根因是 `thickArc` 逐层画独立 1px 圆弧，相邻角度/半径层像素取整后跳格留缝。改为 `ArcThick` 径向填充（角度按外圆弧长自适应细分 + 每角度沿半径逐像素连填）。加 `GY_Sin64/Cos64`（1/64 度插值）。`test_arc` 验证整环+半环带内严格无洞。
- **键盘+焦点系统**：`GY_EVENT_Key/FocusGot/FocusLost`、`GY_STATE_Focusable`、`YMGUI_SetFocus`、`ctx->last_key`。SDL 转发 `SDL_TEXTINPUT`(可打印) + `KEYDOWN`(控制键→GY_KEY_*)。
- **TextInput**：文本缓冲+光标，聚焦绘光标+高亮边框，处理插入/退格/Del/左右移。`test_textinput` 8 项全过。

---

## 第 8 轮：审计与修复（重要）

代码量到 30+ 文件后，用 3 个 subagent 并行审计（控件层/图元层/对象树），逐条核对后修复真实问题：

- **A. malloc 失败 NULL 解引用（系统性）**：`gy_assert` 只打印不中止（release 下是空宏），所以 `GY_malloc0` 失败后继续解引用必崩，还违背"失败返回 NULL"约定。ctx/obj + 11 个控件全部加了失败检查提前返回。
- **B. band 缓冲区越界（最危险）**：`dirty.w > buf_px_cnt` 时 `band_h` 被钳成 1 却仍写 `dirty.w` 像素 → 越界。直接违背"buffer 可小于屏"的设计承诺。改为横向+纵向双向切块。加 `test_band`（buffer 只给 100px、屏宽 320）专门验证不越界。
- **C. 图元越界防御**：`GY_PutPx`/`DrawFill`/`DrawText` 原只裁到 clip，靠 `clip==buf_area` 隐式不变式续命 → 加 `∩buf_area` 边界。
- **D. int32 溢出**：Slider/Bar/Meter/Arc 值映射 + ArcThick，大 range/大半径溢出 → 全改 `(int64)` 中间量。
- **E. 矩形求交 int16 截断**：`x+w` 超 32767 截断成负 → 求交误判 → 内部用 int32 临时量。

**问题 6（自己的测试 bug 两处）**：(1) `test_band` 没先刷掉创建产生的初始全屏脏区，导致定向测试的脏区被合并；(2) 残留一个不存在的 `screen_full()` 调用致编译错误 exit 2。都修正。

审计结论：**没发现逻辑/架构问题**，全是边界健壮性（内存失败/越界/溢出）——说明前 7 轮的核心设计站得住，问题都在"极端输入下的防御"层。

---

## 第 9 轮：滚动机制 + List 容器

- **对象树加滚动+子裁剪**（通用机制）：`scroll_x/y` + `GY_STATE_ClipChildren`；`GetAbsArea` 减父 scroll；`drawObjRec` 对 ClipChildren 父收窄 clip。`test_scroll` 验证偏移+裁剪+滚出不可见。
- **List 可滚动容器**：拖动改 scroll_y（钳制），条目 event_cb 把拖动转发父列表（child 捕获也能滚）。`test_list` 验证内容高累加、两端钳制、拖动双向。

**问题 7（又是自己的测试逻辑）**：`test_scroll` 的 flush 统计把非零像素都算，而视口背景 `0x202020` 在 RGB565 非零，导致"滚出后无内容"误判。把视口背景改纯黑(RGB565 的 0)后只统计白色子项才准。

---

## 关于环境与验证方法

判定 build/test 成败以命令 exit code 为准,别靠 stdout 里的文字。原因是工程原则——stdout 会混入任意文本(测试用例故意打印的 FAIL/assert 字符串、Claude Code 对搜索/文件类工具输出无条件加的通用防注入 reminder),这些都不是"外部对手实时篡改返回"。**此前记录里"工具输出被注入伪造文本"的说法夸大了,已纠正**:没有对抗性注入的证据,不必过度反应,只需坚持看 exit code。

需要区分两类"失败":
- **真失败**(要修):编译器给出带文件:行号的 error,exit code 非 0。本文档里的问题 2/4/6/8 都是。
- **假失败**(忽略):输出中间插入的无行号、无法复现的"崩溃/限流/放弃"文本。

多次用 `echo "exit=$?"`、逐个跑测试 bin 看退出码、grep 编译错误行号来交叉验证真实状态。所有记录在案的"通过"都是真的。

---

## 累计状态

| 轮次 | 主题 | 测试数 |
|------|------|--------|
| 1 | 地基 | 1 |
| 2 | GUI 核心 | 2 |
| 3 | 多矩形脏区+字体+Label | 5 |
| 4 | 表单四件套 | 6 |
| 5 | 图元层 | 7 |
| 6 | 显示控件 | 8 |
| 7 | 粗弧+键盘+TextInput | 10 |
| 8 | 审计修复 | 11 |
| 9 | 滚动+List | 13 |

当前：13 控件、5 类图元、13 单测全过、7 个 demo、约 4300 行库代码，clean build 0 错 0 警。

---

## 第 10 轮：Chart 折线图

- **Chart 控件**：网格(hdiv 横/vdiv 竖等分) + 多序列(固定上限 4 条)折线。y 值按 `[min,max]` 映射到高度(翻转:大值在上)，点沿 x 均布。`AddSeries` 返回索引/满返 -1；`SetValue` 设某点(钳制)；`SetNext` 整体左移末尾入新值(流式滚动)；`SetRange` 收窄值域钳既有点；`SetPointCount` 重置所有序列点数组；`SetGrid` 设网格分格数。
- **绘制**：背景 → 网格 → 各序列折线(相邻点 `DrawLine` 连接，映射用 `int64` 中间量防溢出)。`free_cb` 级联释放各序列点数组。
- **test_chart** 验证：SetValue 钳制 / SetNext 左移 / SetRange 钳既有 / 序列上限拒绝 / GetValue 越界返 min、折线用序列色渲染出像素。
- **demo_chart**：两序列流式(正弦 + 锯齿，定点三角表 Q15)每帧 SetNext 滚动。

**问题 8（边界映射越界，用户发现）**：demo 运行时 Chart 灰框外出现永远不刷新的黄点和蓝色竖线 → 渲染越界到控件矩形外，后续刷新不覆盖那些脏像素。根因：
1. `idxToX` 最后一点：`abs->x + abs->w * i / (point_cnt - 1)` 当 `i = point_cnt - 1` 时算出 `abs->x + abs->w`，越出右边界 1px。
2. 网格线同理：`abs->w * i / d->vdiv` 最后一格越界。

修法：映射改用 `(abs->w - 1) * i / ...` 和 `(abs->h - 1) * i / ...`，保证结果严格在 `[abs->x, abs->x+abs->w-1]` 范围内。删除了多余的 `if (gx >= ...) gx = ...` 钳制(映射已限界)。

测试调整：原断言"斜线像素 > 平线像素"对边界收缩敏感(6 点折线横向少 1px 可能导致翻转)，改为只验证"平线也画出像素"(更鲁棒)。

---

---

## 第 11 轮：top_layer 弹出层机制 + Dropdown

Tabview/Dropdown/Table 三个待办控件都卡在同一块缺失的地基上：**z-order 只在兄弟链内成立**（靠后的兄弟在上层，draw 和 hit 都遵守），但无法跨子树逃逸——深埋在容器里的弹出内容既盖不到全屏之上，又会被父的 `ClipChildren` 裁掉。参考 LVGL 分层，先补这块地基。

- **top_layer(OPOBJ)**：`GYctx` 新增 `top_layer` —— 全屏透明容器，`parent=NULL`(与 root 同级)、`draw_cb=NULL`(不画自己故 root 内容透出)。`Ctx_Creat` 建它、`CtxFree` 先拆其子树再释放它。新增 `YMGUI_Ctx_GetTopLayer` 取句柄、`YMGUI_Obj_SetHidden` 显隐(标脏顺序：隐藏时先标脏再置位，显示时先清位再标脏，因 Invalidate 对 Hidden 对象直接返回)。加 `GY_OBJ_Dropdown` 类型。
- **渲染(Invalidate.c)**：每条 band `drawObjRec(root)` 之后再 `drawObjRec(top_layer)` → 弹出层永远盖在最上。
- **命中(Event.c)**：`HitTest` 先测 top_layer 子(它在最上，命中也应优先)，但只有命中到某个**子**(而非透明容器本身)才采信——否则全屏透明容器会吞掉所有点击，落空则回落 root。
- **Dropdown 控件**：合起=选中项+下拉箭头(两段线画 ∨)。点击在 top_layer 弹出：全屏透明 **backdrop**(先建，在下层，菜单外点击收起) + 浮动 **menu**(后建，在上层，含各选项行)。菜单默认贴本体正下方，屏幕下方放不下则**向上翻**(读 `disp->ver_res` 判定)。选项点击→定选中+触发回调+收起；菜单外点击经 backdrop 收起。
- **生命周期要点**：弹出层挂 top_layer，与 dropdown 本体**不在同一子树**，故本体 `free_cb` 显式 `teardownPopup`。**收起只隐藏不释放**(选项/遮罩回调仍在该对象事件派发中，释放会 use-after-free)；**展开时才拆旧建新**(此刻由"点击合起态本体"触发，弹出层对象不在调用栈上，释放安全)。
- **test_dropdown** 验证：top_layer 存在且初始空、AddOption 返下标、SetSelected 越界忽略、**z-order**(菜单展开后采样菜单中心像素≠底下 root 红块、收起后红块复现)、选项点击定选中+回调 sel 正确、外部点击收起且不触发回调、**逃逸 ClipChildren**(把 dropdown 塞进开裁剪的小容器，菜单仍完整浮出、命中落在浮动选项行而非容器/root)、析构不崩。
- **demo_dropdown**：三个下拉(顶部朝下、右侧、底部向上翻)，选色同步到状态标签。

测试增至 ctest 15/15 全 PASS，9 个 demo 无头跑通，clean build 0 错 0 警。

---

---

## 第 12 轮：Table 表格

第三个待办控件。选择**自绘型**(单 draw_cb 画完全部单元格,类似 Chart),而非像 List 那样给每格建对象——N 行 x M 列在裸机上逐格建对象太重。数据内部存:列定义数组 + 行链表(尾插 O(1),无需 realloc)。

- **数据模型**:`GYtbl_col[8]`(标题+列宽) + `GYtbl_row` 链表(每节点 `cells[8][24]`)。列上限 8,格文字上限 24。
- **绘制(复用三处)**:背景 → 表体 → **sticky 表头**(最后画,盖在滚上来的行之上)。
  - 复用 **Chart 网格**:列竖线 + 行底分隔线。
  - 复用 **ClipChildren 思路**:draw_cb 先把 `s->clip` 收窄到自身区域(自绘控件手动做子裁剪的事),表体再收窄到 `[表头之下, 视口底]` → 滚动的行不会盖表头、不溢出视口。
  - 单元格文字也各自把 clip 收到本格,防长文串到邻格。
  - 只画可见行:首个可见行 = `scroll_y / row_h`,滚出下沿即 break。
- **交互(复用 List 拖动语义)**:按下记锚点(y+scroll),Pressing 时 `scroll = 锚点scroll + (锚点y - 当前y)` 钳制。**行点击选中带拖动阈值**:按住累计位移 > 4px 判为滚动,抬起(Clicked)时不算行点击——区分"选中"与"滚动拖拽"。点表头不选行。
- **API**:AddColumn/AddRow/SetCell/GetCell、SetRowHeight、SetScroll/GetScroll、SetSelectedRow/GetSelectedRow、SetRowCb。free_cb 级联释放行链表。
- **test_table** 验证:列/行 API 顺序下标、单元格读写+越界返空、滚动两端钳制(内容220-视口88=132)、**sticky 表头**(滚到底表头区仍是表头色)、**行体裁剪**(选中行滚出上方后表头区无选中色泄漏)、行点击命中正确行+回调、点表头不选、**拖动超阈值不算点击**(选中不变、scroll 按位移移动)、析构不崩。
- **demo_table**:3 列 x 30 行,拖动滚动 + 单击选中同步行号到标签。

测试增至 ctest 16/16 全 PASS,10 个 demo 无头跑通,clean build 0 错 0 警。

---

---

## 第 13 轮：Tabview 标签页

第 11 轮埋的伏笔终于兑现:切页**直接复用 `YMGUI_Obj_SetHidden`**,几乎不写新机制——隐藏页被 draw / hit-test 双双跳过(第 8 轮 drawObjRec 和 hitRec 都已判 Hidden 位),所以切页只是"隐藏旧页 + 显示新页",天然干净、无需 z-order 处理。三个卡在弹出层/显隐地基上的待办控件到此全部收完。

- **结构**:顶部 tab bar(等分分段,当前页高亮底色 + 白字 + 底部 2px 蓝下划线) + 下方内容区。每 tab 一个**页容器**(内容区的子对象,开 ClipChildren),`AddTab` 返回它给用户往里加控件。非首页 AddTab 时直接置 Hidden 位(尚未显示无需标脏)。
- **绘制**:draw_cb 只画 bar(内容由各页自绘)。末段补足整除余宽避免右侧留缝。
- **切页**:tab bar 点击落在 tabview 自身(页在 bar 之下),由 `point_x - abs.x` / 段宽算命中 tab;`SetActive` 隐藏旧页(标脏露出底层)、显示新页、标脏 bar(高亮变)。点当前页 tab 不切、不触发回调;内容区点击留给页内控件。
- **API**:AddTab(返回页容器)、SetActive/GetActive、GetTabCount/GetPage、SetBarHeight(重排各页)、SetChangedCb。free_cb 只释放自身数据——各页是子对象,随对象树级联释放(不重复释放)。
- **test_tabview** 验证:AddTab 返回页、GetPage 越界 NULL、初始仅第 0 页可见(内容区采样红色)、**隐藏页被 hit-test 跳过**(内容区点击命中当前页子而非隐藏页子)、点 tab 切页(采样色变红→绿→蓝)+ 回调 idx 正确、点当前页 tab 不触发回调、内容区点击不切页、SetActive 程序切页 + 越界忽略、析构级联不崩。
- **demo_tabview**:3 页(Home 放标签+按钮 / Settings 放两复选框 / About 放文字),点 tab 切页,页内控件随页显隐。

测试增至 ctest 17/17 全 PASS,11 个 demo 无头跑通,clean build 0 错 0 警。

---

## 第 14 轮：状态/数据绑定地基（数据驱动第一刀）

控件收完后转向用户的核心诉求——**数据驱动:UI = f(state)**。界面只是状态的外显,状态是唯一真相,后端只改状态、界面自动跟。设计讨论定了三个基调:

- **否决路径注册表**。状态用静态符号,绑定期直接给 `&subject`,运行时纯指针无查表。路径(字符串寻址)只是将来做 description-driven UI(界面从表/配置长出来)时可选加的薄壳——解析回的还是同一个 `Subject*`,后加不破坏任何东西,现在不建。
- **标量优先**。结构状态(列表/表格整行绑定)是另一个模型,暂不碰。
- **纯上层**。写回接口复用现成的:slider 的 `changed` 回调 + `updateFromPointer` 里已有的 compare-and-skip;label 的 `SetText`。两个方向控件都留好口子。

实现:

- **新目录 STATE/**(进 CMake include + GLOB)。三原语:`GYval`(带标签标量联合)、`GYsubject`(权威值 + observer 链 + 重入位,`GY_SUBJECT_INT/BOOL/FIXED/STR` 宏可静态声明零堆)、`GYobserver`(subject/target/apply/next 头插链)。
- **keystone 硬改**:GYobj 加 `void* bind_data`(与 user_data 分开)。`YMGUI_Free_ObjFree` 加一步——bind_data 非空则 `YMGUI_Bind_Unlink`(前置声明,不让 OPOBJ 头依赖 STATE),控件销毁自动从 subject 链摘掉,杜绝野指针。无全局状态。
- **断环**:Set* 做 compare-and-skip(值同不通知)+ notifyAll 用 notifying 位防重入。字符串 Str 只换指向不拷贝,原地改 buffer 需显式 `Touch`。
- **控件适配器**(`YMGUI_Bind.h/.c`):每控件一个 `*_Bind`(对标 LVGL v9 `lv_xxx_bind_value`——slider/switch 都是 GY_OBJ_Base 无法靠 type 分派)。Label_Bind 单向(本地 fmtInt 整数→串,不引 sprintf 浮点);Slider_Bind 双向(apply 走 SetValue 天然断环,writeback 经 bind_data 拿 subject 写 State_SetInt)。**注:Slider_Bind 占用 changed 回调,绑后勿再 SetChanged**。
- **test_bind** 验证:绑定即同步当前值、subject→多控件扇出、拖 slider 写回 subject 且扇出另一 slider(双向不死循环)、free 一控件后 subject 仍驱动另一个(摘链安全)、重复写同值稳定、字符串换指向刷新/空串清空、静态声明 subject 可用、ctx 释放后再写 subject 不崩。踩坑:点击 x=110 落 slider 右边界外(GY_Rect_Contains `x < x+w` exclusive),改 x=108——测试坐标 bug 非绑定 bug。

测试增至 ctest 18/18 全 PASS,clean build 0 错 0 警。

**铺满适配器(同轮)**:把绑定推到其余基础控件——
- **Switch / Checkbox**(双向 bool):`changed` 只在点击触发、`SetOn`/`SetChecked` 不回触发 → apply 天然断环;writeback 走 State_SetBool。
- **Bar**(单向 value):只显示无 event,只 apply 不写回。
- **TextInput**(双向 str):原本无变更钩子,**补了 `YMGUI_TextInput_SetChanged`**——按键处理里比较 len 前后,仅内容真变(编辑,非纯移光标)才触发。绑定处理两个字符串难点:(1)**自回声保护**——写回后 subject 指向文本框内部缓冲,再 apply 会自拷 + 光标跳末尾,故 apply 里判 `s == GetText(ti)` 则跳过,保住编辑中光标;(2)**原地改通知**——缓冲地址稳定,SetStr 的 compare-and-skip 会跳过后续变更,故地址不变时用 `Touch` 强制通知其余观察者(如绑同 subject 的 label)。
- test_bind 扩充:switch 点击写回 + SetBool 刷新;checkbox/switch/bar 三控件绑同一 bool subject,点 checkbox 扇出到另外两个(含异构 bool→bar value);textinput 键入写回 + Touch 通知 + 扇出到 label + 后端 SetStr 反向驱动。ctest 仍 18/18(test_bind 内新增断言),clean build 0 错 0 警。

现七个基础控件可绑:label(单向 int/bool/fixed/str)、bar(单向 value)、slider/switch/checkbox(双向)、textinput(双向 str)。

**demo_bind(同轮)**:数据驱动手感 demo,与 demo_form 正好对照——demo_form 用 `onSliderChanged` 回调手动 poke 进度条+标签,demo_bind **零胶水回调**,全靠绑定。三个 subject 演示三种数据流:control(int,拖滑块→写它→bar+label 扇出跟随,UI→state→UI)、sensor(int,后端每 6 帧推三角波→bar+label 自动跟,state→UI 全程不碰控件)、power(bool,开关与复选框绑同一个→点任一另一个同步,UI↔UI)。主循环里后端只调 `YMGUI_State_SetInt(&sv_sensor, ...)`,从不碰控件——这就是"界面只是状态外显"的落地。headless(SDL dummy)跑通 exit 0。

**收尾:app 观察者(同轮)**:补 `YMGUI_State_AddObserver(s, cb, user_data)` / `RemoveObserver`——让**业务逻辑直接订阅状态**,而非监听控件的 changed 回调。这是"控件绑定与自定义逻辑并存"的正解(初判要碰各控件 .c 让 changed 与绑定共存,核 LVGL 后否掉:数据驱动里 app 该监听状态不该监听控件)。实现只加 STATE 层零控件改动:observer 节点加 `notify`/`user_data` 两字段,`target==NULL` 走 app 分支;notifyAll 里先存 next 再回调(容忍回调内自摘)。语义优于 changed 回调——状态变化不论来自用户交互 writeback 还是后端 SetXxx 都触发,且控件的 changed 槽仍归绑定专用,天然并存。test_bind 增:AddObserver 不立即触发、后端 SetInt 通知 + user_data 透传、值未变不通知、slider 与 app 观察者共 subject(拖拽写回两者都动)、RemoveObserver 后不再通知而控件观察者仍活。ctest 18/18,clean build 0 错 0 警。

---

## 第 15 轮：抗锯齿(可裁减)

核心认知:AA = **coverage(灰度覆盖率 0~255)** + **alpha 混合**。没有独立"彩色 AA"——coverage 当 alpha 把前景色混到 framebuffer 已有背景色上,彩色自然出来。子像素渲染(ClearType)不做(依赖面板子像素排布、旋转即废、有彩边、3× 开销,裸机不值)。

**A. 混合基石(keystone)**:原 `blendPx` 是 DrawFill.c 的私有 static。提升为 DrawPx.h 的共享 static inline——`GY_MixPx(dst,src,opa)` 纯像素混合(按 COLOR_DEPTH 分支,565 解包线性混合再打包)、`GY_BlendPx(s,x,y,color,opa)` 带裁剪+band 的读-混-写回(opa=0 跳过、255 直写、其余混合,逻辑对齐 GY_PutPx)。DrawFill 反过来复用 GY_MixPx(纯去重,行为不变)。所有 AA 图元共用此内核。

**B. 几何图元 AA**(全包 `#if YMGUI_ANTIALIAS`,`#else` 保留原整数直写):
- **Wu 反走样直线**:斜线沿主轴步进,副轴 16.16 定点,主/邻像素按分数分覆盖度混合;水平/垂直走原快路径(无锯齿不混合)。**修了一处真 bug**:覆盖度公式 `((65535-frac)*255)>>16` 在 frac=0 时截断成 254 而非 255,导致满覆盖端点也走混合、丢失锐利精确色(test_chart 精确色采样一度归零)。改成互补式 `opaMain=255-opaNext`,frac=0→255 保持锐利。
- **圆**:Wu 圆——第一八分圆逐 x 算 `y=sqrt(r²-x²)`(新增 `GY_Isqrt` 整数平方根,放 COMMON/MATH,逐位法无 FPU;放大 256 倍取 8 位小数),主/邻像素分覆盖度,8 对称。
- **实心圆**:逐行实心填到 floor(半宽),左右边界各混合一像素(coverage=小数)。内核精确色、边界平滑。
- **弧 / 粗弧环**:按弧长细分(deg64 定点三角),端点用**双线性 splat**(四邻按权重混合)抹平锯齿;粗弧环内部保持实心,只对内外两条边界半径 splat。

**C. 字体 4bpp 灰度 AA**:`GYfont` 加 `bpp` 字段(1=硬边点阵,4=灰度覆盖度)。`Draw_Char`:4bpp 每字节含 2 像素(高 nibble 在左),nibble×17→覆盖度 0..255 走 GY_MixPx;`YMGUI_ANTIALIAS==0` 时 4bpp 字体退化阈值(nibble≥8→直写),保证开关一键裁掉所有混合;1bpp 走原取位直写(向后兼容,裸机可回 1bpp 省 flash)。gen_font.py 去掉 `>=128` 阈值,改把灰度量化到 4bit(`px>>4`)两像素打包一字节,加命令行 bpp 参数(默认 4)。FontData.c 重生成:8×16 字模 16B/字 → 32B/字(6080B),bytes_per_row=4,bpp=4。

**开关**:CONFIG/YMGUI_PubType.h 加 `YMGUI_ANTIALIAS`(默认 1),与 `YMGUI_COLOR_DEPTH` 同级;`COLOR_DEPTH==1`(单色屏无中间灰度)时内部 `#undef` 强制置 0。

**验证**:新增 test_aa(混合基石半覆盖出中间值 + 裁剪外不写;Wu 斜线有 AA 中间像素且端点精确色;水平线仍纯色;圆/实心圆/弧边界有中间灰度、实心核精确;4bpp 'A' 有灰度边、空格全零)。ctest **19/19**。**可裁减验证**:`-DYMGUI_ANTIALIAS=0` 全新构建亦 19/19、0 警(AA 断言已 `#if` 守卫,4bpp 字体走阈值路径)。

---

## 第 16 轮 — 头文件职责拆分 + 三处 AA bug 修复

**头文件拆分**:`PubType.h`/`PubDefine.h` 职责分清——"是什么"进 PubType(纯 typedef + 选宽开关 COLOR_DEPTH→GYpx、COORD_32→GYcoord),"怎么算/配多大"进 PubDefine(YMGUI_ANTIALIAS 及其单色屏强制关、GY_FP* 定点宏、GY_OPA_*、GY_INV_MAX)。留面包屑注释指向对方。OPOBJ/YMGUI_Obj.h 补 `#include "YMGUI_PubDefine.h"`(用了 GY_INV_MAX)。

**三处 AA bug(同一类根因)**:填充/粗描边图元不能靠"实心 + 边缘补淡"抗锯齿,必须用**距离场**(cov = r*256+128 − dist*256,各方向一致)。
- demo_draw 同心圆环中间发黑:Wu 圆羽化写错侧(邻像素落在 yi−1 内侧,应 yi+1 外侧,真边界在外)→ 修 DrawArc.c:85。
- demo_draw 红色实心圆上下不正常:CircleFill 只在左右边羽化,上下(平边)硬实心 + 宽度突跳 → 距离场重写(实心核到 r−0.5,边缘环距离场)。
- demo_dashboard 两蓝圈无 AA:ArcThick 两条径向边是实心楼梯,splat 补不回来 → 距离场重写(外/内半径取 min)+ 半平面叉积角度判定。

## 第 17 轮 — 中文/CJK 字体(UTF-8,可裁减)

**回退链**:GYfont 加 `fallback` 字段,默认 ASCII 字体(8×16 连续 32..126)遇 CJK 码点自动回退到 YMGUI_Font_CJK(16×16 4bpp,稀疏排序 uint16 codepoints[] + 二分)。所有 20 处控件调用点(全用 `&YMGUI_Font_Default` + `const char*`)零改动即得中文。

**UTF-8 + 稀疏字模**:DrawText.c 重写——utf8_next 解码 1~4 字节,glyphIndex 连续或二分,resolveGlyph 走回退链,Draw_Char 成 Draw_Glyph 薄壳。

**外部 flash**:GYfont 加 `glyph_read` 回调 `(font,off,len,buf)`,NULL=直接指针(内部/映射 flash),置了=拷到栈 buf 再 blit(非映射 SPI flash)。一个 16×16 4bpp 字模 128 字节。

**字模来源可选**:gen_font.py 重写(argparse),预置字集(方案1,精简 flash)或 GB2312 全集(方案3,`--gb2312`,可放外部 flash)。CJK_TTF=NotoSansCJK-Regular.ttc。

**开关**:`YMGUI_FONT_CJK`(默认 1)。置 0 时回退链断开、FontDataCJK.c 全文 `#if` 守卫编译成空、FontData.c 里 `&YMGUI_Font_CJK` 引用亦在守卫内→无悬空符号,回到纯 ASCII 足迹。

**验证**:test_font 加中英混排宽度("A中"=24、"中文"=32)、Draw_Glyph 命中/缺字(0x9FA5 进 8 不出图)、外部 flash mock(read_calls==1、出像素)测试(均 `#if YMGUI_FONT_CJK` 守卫)。新增 demo_font_cjk(标签/按钮直接塞 UTF-8 中文,headless 跑通)。clean build 0 警、ctest 19/19;`YMGUI_FONT_CJK=0` 裁减构建亦 0 警、test_font 全过。

**光栅化修正 + 决策文档化(第 17 轮补丁)**:
- **矮半截 bug**:emit_cjk 原 `PT=15 / Y_OFF=0`,字沉进格子下半(ink 在 5..15 行)且底行被裁,只剩 ~11px/16px → 用户报"中文比英文矮半截、少半截"。改 `PT=16 / Y_OFF=-5`,字模填满格高(1..15 行)、与 ASCII 基线对齐。ASCII 探针逐字确认无裁切。
- **缺字非 bug**:二三/左右/大小人天本就不在 PRESET_CJK 里,fallback 正确地不出图(字集范围问题,非渲染 bug)。给 PRESET_CJK 补 上下左右/大小人天/一~十 等,59→75 字。
- **三轴决策成文**:CJK 开/关(编译期宏 `YMGUI_FONT_CJK`)、字集范围(生成期 gen_font.py 参数,**非宏**——预处理器变不出字模,方案1/3 是数据不是代码)、字模存放(运行期 `glyph_read` 字段)。写进 CONFIG/YMGUI_PubDefine.h 头注释 + gen_font.py 脚本头 + API.md 表格。**要点:方案1/3 不该用宏裁决**。

**方案3 + 外部 flash 规模验证(第 17 轮补丁 b)**:
- **运行期全局兜底钩子**:DrawText.c 加 `YMGUI_Font_SetFallback/GetFallback`(一个 `.bss` 指针,默认 NULL)。任一字体的 const `fallback` 链走空后,resolveGlyph 再下探这个全局字体(也走它自己的 fallback 链)。控件仍用 `&YMGUI_Font_Default`,零改动即得任意中文。字体数据本身仍全 const,其余 17 目标只 +4 字节 `.bss`。
- **索引/字模分离**:gen_font.py 加 `--extern`,GB2312 全集拆成【固件内索引.c(`YMGUI_GB2312_cps[]` 6763 码点 + `glyph_count`,~13.5KB,无 bitmap)】+【`gb2312_glyphs.bin` blob 865664 字节=6763×128,第 i 字在 i*128 偏移,烧进外部 flash 的内容】。加 `--bin` 指定 blob 输出路径(可复现,免手搬)。app 侧自造 GYfont 填 `glyph_read` 回调,不依赖库内 GYfont 定义。
- **回调即移植缝**:库永不碰通讯协议。demo_font_gb2312 的 `flashRead` 用 fopen/fseek/fread 从磁盘 blob 取字节(模拟外部 SPI flash);真机只改函数体为 `W25Q_Read(BASE+off, buf, len)`。CMake 用 `target_compile_definitions` 把 blob 绝对路径塞进 `GB2312_BIN_PATH`。
- **验证**:新增 demo_font_gb2312,全用方案1 精简集之外的字("欢迎光临智能仪表盘/谨慎驾驶/剩余里程/保存配置/退出登录"),headless 跑通 exit 0、flash reads=40>0(外部路径真被走)。像素探针确认出格外字"欢"(0x6B22)ink=128 填满 16×16、adv=16。clean build 0 警、ctest 19/19。

---

## 第 18 轮 — 异步双缓冲(buf2 ping-pong + DMA 重叠)

**目标**:让 flush(帧传输到屏)与下一段的 CPU 绘制重叠,榨干带宽。裸机上 flush 通常是 SPI/DMA 长事务,同步等它跑完就白白浪费 CPU。

**职责切分(关键决策)**:接口原语归 HAL,调度状态机归 GUI。
- **HAL(`YMGUI_Hal.h`/`.c`)只提供原语**:GYdisp 追加两个字段——`volatile uint8 flush_busy`(库发起异步 flush 前置 1)、`void (*wait_cb)(struct GYdisp*)`(等 busy 清零时调,裸机可填 `__WFI`;NULL=忙等自旋)。`YMGUI_Disp_FlushReady` 由空实现改为 `d->flush_busy = 0`——真机在 DMA 完成中断里调它。HAL 不认识"band/条带"是什么,不做调度。
- **调度状态机在 GUI(`YMGUI_Invalidate.c`)**:`buf2 != NULL` 才激活乒乓;否则完全走老同步路径(19 个存量测试一律 buf2=NULL,零影响,已验证有的测试根本不调 FlushReady 也照过)。`refreshOneRect` 签名改为带 `GYpx** pcur` 游标 + `int dbl = (disp->buf2 != NULL)`。每条带:`s.buf = *pcur`;异步则 `waitFlushIdle(disp); flush_busy=1; flush_cb(...); *pcur = (*pcur==buf1)?buf2:buf1` 切缓冲;同步则原样 flush。
- **不变量**:至多 1 个传输在飞;绝不写正在传输的缓冲。`waitFlushIdle` 自旋 `flush_busy`(有 wait_cb 就调,让裸机进低功耗等中断)。`YMGUI_Refresh` 循环前排空(跨帧防覆盖上一帧仍在传的缓冲)、循环后排空(退出前 drain)。

**桌面单线程测法(`test_async.c`)**:mock DMA——`asyncFlush` 只 `g_inflight++` 记录并检查乒乓(相邻缓冲地址不同),**不**调 FlushReady;`asyncWait`(即 wait_cb)`g_inflight--` 后 `YMGUI_Disp_FlushReady(d)`,模拟"DMA 完成中断"。断言:flush 次数>1、缓冲交替 ok、max_inflight≤1、跑完 inflight==0 且 flush_busy==0、脏区覆盖==全屏。附带 buf2=NULL 单缓冲回归(syncFlush)。

**验证**:clean build 0 错 0 警,ctest 20/20 全过,test_async ALL PASS。

---

## 第 19 轮 — 数据绑定铺到仪表类(Arc/Meter 单向)

**背景纠偏**:DEVLOG 待办里写的"数据绑定铺开(switch/checkbox→bool、bar→value、textinput→str)"其实第 14 轮就做完了(6 个适配器:Label/Bar 单向 + Slider/Switch/Checkbox/TextInput 双向),那条是陈旧文字。本轮铺的是**尚未绑定、有值/选中语义的控件**里的低风险一档。

**范围决策(问过用户)**:候选有单向 Arc/Meter(value)+ 双向 Dropdown/Tabview/Table(选中 index)。用户选**只铺单向 Arc/Meter**——照抄 Bar 模式,零风险、明显有用(传感器 subject → 仪表盘/环形进度自动跟);index 绑定按需再说。

**双向那三个的可行性(顺带查清备用)**:Dropdown_SetSelected / Tabview_SetActive / Table_SetSelectedRow 全是"只改状态 + Invalidate,**不触发各自的 cb**",跟 Slider/Switch 一个断环模式 → 将来要铺 index 双向绑定,方案成立。

**实现**:`YMGUI_Arc_Bind` / `YMGUI_Meter_Bind`,各一个 apply(`Arc/Meter_SetValue`,SetValue 自带钳制+标脏,无写回)+ `Bind_Attach`。与 Bar 同构,纯单向。

**验证**:test_bind 加一段——Arc+Meter 绑同一 int subject:绑定即同步当前值(30)、后端 SetInt(85) 扇出两者、越界值(200)各控件 SetValue 自钳到 100 而 subject 保持 200(印证单向、无写回)。demo_bind 加 ctl_arc(绑 control,拖滑块跟)+ sen_meter(绑 sensor,后端三角波摆)。ctest 20/20(扩的是既有 test_bind,未新增测试文件)、clean build 0 警、demo_bind headless exit 0。

---

## 第 20 轮 — 轻量一次性布局助手(Stack + Align)

**动机**:此前每个控件都手写 x/y/w/h,加/挪一个控件就要重排一片坐标(往 demo_bind 塞 Arc/Meter 时就得拿计算器算)。List/Table 各自造了一遍"竖直堆叠"的轮子,该提炼成通用工具。

**取舍决策(问过用户,选"轻量一次性助手")**:不做 LVGL 那种 flex/grid 引擎——它要每对象存约束、每帧 measure→arrange→reflow、代码几 KB,与裸机优先/固定屏尺寸拧着来。做**无状态一次性布局**:调用时按当前尺寸算好坐标写进现成的 `area` 字段,标脏一次返回,零每对象 RAM,布局变动时调用者重跑(不像 reactive 引擎每帧算)。这与"异步双缓冲用运行期字段而非宏""数据绑定标量优先不做集合"同一套哲学——够用的最小机制,不背用不上的复杂度。

**放哪层**:新模块 `GUI/YMGUI_Layout.{h,c}`。布局遍历对象树(`child_head→sibling`)、写 `area`、调 `YMGUI_Obj_Invalidate`(都在 GUI 层),与 Refresh/Invalidate 同属"操作整棵树的 GUI 工具"。**库侧 CMake 零改动**——`GLOB GUI/*.c` 自动纳入(只有 test/demo 可执行文件要手动加)。

**API(无状态,只定位不缩放)**:
- `YMGUI_Layout_Stack(parent, dir, gap, pad, cross)` —— 子对象沿主轴(VER/HOR)依次排布:主轴游标从 pad 起、每个子 += 尺寸+gap;交叉轴按 cross(START/CENTER/END)对齐。**跳过 Hidden 子(不占位)**,只写位置不改 w/h。结尾标脏 parent 一次(其绝对区覆盖所有子的旧+新位置,省得逐子 old/new 两趟)。
- `YMGUI_Layout_Align(obj, align, pad)` —— 把 obj 在其父内容盒里做九点对齐(CENTER/TL/TM/.../BR),pad=离边内缩。

**可裁**:`YMGUI_LAYOUT` 宏(默认 1,PubDefine.h),`#if` 包住头声明和 .c 全体;库核心无处调用它 → 关掉无悬空符号。已实测 `-DYMGUI_LAYOUT=0` clean build 0 警、test_layout 仍链接(断言 `#if` 守卫,打印"compiled out"照过 exit 0)。

**验证**:新增 test_layout(竖排 y 累进 10/35/60、横排 x 累进 8/52/96、交叉轴 START/CENTER/END、Hidden 子不占位、加子后重排、Align 九点)。新增 demo_layout(设置面板全靠 Stack/Align 排布,零手写坐标:标题 Align 顶部居中、面板竖排两组开关、底部按钮横排),headless exit 0。ctest 21/21、clean build 0 警。

---

## 第 21 轮 — 可滚动多行文本视图(TextView)

**动机**:此前全库没有多行文本控件——`YMGUI_Draw_Text` 明确"不处理换行",Label/TextInput/List 都是单行。要显示一段日志/说明/文件内容(带 `\n`,或长到需要按屏宽折行)没有现成控件。文件管理器的"文件列表"本身用 List/Table 就够,缺的是"多行只读文本块"。

**取舍决策(问过用户)**:用户选"可滚动文本视图,而多行(按 `\n`)和自动折行作为它的两个可选属性"。于是做**一个**控件 `TextView`,两个独立开关:`Multiline`(遇 `\n` 断行,默认开)、`Wrap`(按控件宽贪心折行,默认关;关掉则长行右侧裁掉、不做横向滚)。编辑光标/富文本多色段/横向滚动明确不做(编辑归 TextInput,横滚在裸机固定窄屏收益低)。

**架构(仿 Table 自绘型)**:`type` 仍是 `GY_OBJ_Base`——自绘控件靠 `user_data`+`draw_cb` 区分,不占新枚举(与 Table 一致,省一处全局改动)。控件持文本副本(SetText 深拷进 `GY_malloc1`),把换行位置预算成**行表**(`{offset, byte-len}` 动态数组,`GY_malloc0`+`realloc0` 扩容),**只在文本/属性/宽度变化时重算**,不是每帧算。draw_cb 收窄 clip 到自身 → 只画可见行(`first = scroll_y/line_h`,滚出下沿 break),复用 Table 的拖动锚点滚动语义(只读,无点击选中)。draw_cb 里比对 `last_w != area.w` 检测被布局改了宽度 → 自动重排行表(所以配合 Layout 助手改尺寸后需 Invalidate 触发重绘)。

**新增 CORE 图元(两个,与现有 API 对称)**:行是 owned buffer 里的**非结尾子串**,而 `Draw_Text`/`Font_TextWidth` 都要 `\0` 结尾,故补按字节长度版:`YMGUI_Draw_TextN(...,nbytes,...)` 和 `YMGUI_Font_TextWidthN(...,nbytes)`。内部 `utf8_next_bounded` 带尾界:多字节序列若会越过 nbytes 边界则退化吞单字节,防读界外。折行贪心用增量 `TextWidthN` 逐码点累加,UTF-8 边界靠 lead byte 数续字节判(不切裂多字节,单码点已超宽也强制放一字防死循环)。

**验证**:新增 test_textview(空文本占位 1 行、`\n` 断 3 行、Multiline 关合 1 逻辑行、Wrap 折行两态、30 字长行折 3 行、滚动钳制、宽度缩放重排 5↔3 行、拖动滚动、文字实际绘制像素非背景)。新增 demo_textview(含 `\n` + 超宽长句的文档,拖动纵向滚动,按钮切 Wrap 看长句折行/裁切两态),headless exit 0。ctest 22/22、clean build 0 警。踩坑:test 里直接改 `area.w` 不标脏 → Refresh 跳过重绘 → 行表不重排,补 `YMGUI_Obj_Invalidate` 模拟真实布局路径(布局助手改完尺寸也应 Invalidate)。

---

## 第 22 轮：TextInput 中文输入修复 + 可编辑多行 EditView

用 project_Demo/txt_edit 做基础验证时暴露的两个缺口:单行输入框打不出中文、缺可编辑的多行控件。

**中文输入根因**:SDL 把 `SDL_TEXTINPUT` 的 UTF-8 **逐字节**注入(一个汉字来 3 个 key,每字节 ≥0x80),而 TextInput 的按键门槛是 `k >= 0x20 && k < 0x7F`——只收 ASCII,所有多字节字节被挡掉,汉字永远进不了缓冲。修复:①门槛放开到 `<= 0xFF`(GY_KEY_* 控制键在 0x1000+,不冲突);②插入仍按字节(逐字节按序拼回整码点),但退格/Del/左右移光标改**按整个 UTF-8 码点**(退格回退扫续字节 0x80..0xBF);③光标渲染从 `cursor * cell_w`(定宽假设,中英混排会错位)改用 `YMGUI_Font_TextWidthN(font, text, cursor)` 按实际像素宽定位。

**EditView**:镜像 TextView 的自绘可滚结构(`GY_OBJ_Base` + 单 draw_cb 裁剪 + 行表 + 拖动滚动),从头把中文做对。定容缓冲(`GY_EV_TEXT_MAX`,裸机不动态扩),光标以字节位记录(始终落码点边界)。编辑:UTF-8 字节插入、ENTER 插 '\n'、退格/Del 删整码点(跨行即删 '\n' 合并行)、LEFT/RIGHT 按码点移、UP/DOWN 按"光标像素列就近对齐"跨显示行。每次编辑重算行表并把光标行滚入可见区。

**测试**:test_editview 覆盖 UTF-8 插入、换行、退格跨行合并、Del、方向键、中文整码点、Wrap 折行、滚动钳制、焦点丢失不吃键;test_textinput 加中文回归块(逐字节注入驱动真实 SDL 路径)。两处一个测试 bug 是我写错了预期(合并后光标已在目标位,多按了一次 LEFT),不是控件问题——控件正确,修的是测试。

**GB2312 两个坑(接真机字库时暴露)**:
- **占位空格 = 字库没这个字,不是输入路径的锅**。内置 `YMGUI_Font_CJK` 只有 75 个预置字形,打任意中文/标点全显示成占位空格。修:走"外部 flash 全字库"那套——固件内只留 `YMGUI_GB2312_cps[]` 排序码点索引,字模在 blob `Demo/gb2312_glyphs.bin`(第 i 字在 i*128,16×16 4bpp=128B/字),app 自造 GYfont 填 `glyph_read` 回调 + `YMGUI_Font_SetFallback()` 挂全局兜底。用像素探针证实:随=0 ink(无 blob)→ 153 ink(挂上)。
- **中文标点全缺 = 字库生成漏了符号区**。`gen_font.py::load_gb2312()` 原来只遍历汉字区 `0xB0A1..0xF7FE`,而中文标点(、。，！？：；（）～""''… )的码点在 **0x3000 / 0xFF00 / 0x2018 段(非汉字区)**,对应 GB2312 的**符号区 0xA1A1..0xA9FE(行 01-09)**——被整段跳过,所以标点一个都不出。修:遍历范围改 `0xA1..0xF8`(补 682 个符号),索引 6763→7445,blob 865664→952960 字节(953KB)。汉字索引对齐不变(随=153 仍成立)。**判据只认 exit code**:重生成后 `touch` 字库源强制重建 libymgui.a,再跑探针 14 个标点全 OK。
- **glyph_count 不能编译期硬编码**:`YMGUI_GB2312_glyph_count` 是 `const uint16` 变量,不是 C 编译期常量,不能进静态结构体初始化器;struct 里填 0,main 里运行期 `s_gb_font.glyph_count = YMGUI_GB2312_glyph_count;` 再 SetFallback。硬编码旧值(6763)会把二分查找截断在前 6763 个排序码点,漏掉后面的字。demo_editview / demo_font_gb2312 / project_Demo/txt_edit 三处均按此接法。
- **破折号 = 输入法产出的码点跟 GB2312 解码表对不上**。索引里有 `U+2015`(HORIZONTAL BAR,Python 把 GB2312 单元 A1AA 解码成的),但中文输入法打"破折号"实际产出 **`U+2014`(EM DASH)**,两码点不同 → 不补就是占位空格。同类:`U+2013`(–连接号)、`U+00B7`(·间隔号)也非 GB2312 码点。修:`load_gb2312()` 末尾追加一个 `EXTRA="—–·"` 补充集,索引 7445→**7448**,blob →953344 字节。像素探针证实三者都出 ink(em=14/en=8/mid=7)。**教训:GB2312 解码产出的码点 ≠ 输入法产出的码点,凡"输入法能打但字库缺"的符号都得显式补进生成集**。

**Tab(用户报"tab 没反应",接着追问"编辑器里 Tab 不是空格吗")**:两处都缺——①SDL_LCD.c KEYDOWN 没转发 `SDLK_TAB`;②库里无 `GY_KEY_TAB` 也无轮转逻辑。加 `GY_KEY_TAB 0x1005` + SDL 转发 + `YMGUI_FocusNext()`(**前序遍历对象树** `focusWalk` 递归 child_head→sibling 零堆,一趟收集 first + 当前焦点后紧跟的 next,目标=`next?next:first` 末尾回卷,跳过 Hidden 子树,焦点被隐藏/摘除时从头选,也遍历 top_layer;只正向,Shift+Tab 未做)。
- **踩坑(设计缺陷,用户点出):Tab 语义上下文相关,不能库层一刀切拦截**。第一版把 Tab 在 `Event_Key` 里提前拦截去轮转焦点,根本没送到控件——可编辑控件里 Tab 该插空格。第一次修:改成**控件优先 + 消费信号**(GYctx 加 `key_handled`,`Event_Key` 派发前清 0、先交焦点控件、控件消费置 1,`Tab && !key_handled` 才轮转)。但只让 EditView 吃 Tab、单行 TextInput 仍轮转——用户再纠正:**"进入编辑状态时 Tab 就该是空格,不在编辑状态(选择态)才轮转"**。
- **焦点两级模型(最终形态)**:加 `GY_STATE_Editing` 位,焦点分**选择态**(仅高亮/Tab 轮转)和**编辑态**(吃 Tab 插空格/编辑文本)。转移:**点击**→聚焦并进编辑(指针意图即编辑,光标现);**Tab 轮转**→`SetFocus` 统一清 Editing,落点永远选择态(旧框失焦即退编辑);**选择态下打字/Enter**→进编辑(键盘 Tab 过来后可直接开打);**编辑态 Tab**→插空格并消费(TextInput 1 个 / EditView `GY_EV_TAB_WIDTH` 个);**编辑态 Enter**→单行 TextInput=结束编辑+直接轮转下一字段(表单手感,少按一次 Tab),多行 EditView=换行(它靠点击/选择态 Tab 离开)。光标仅编辑态显示(选择态只高亮),两级状态肉眼可辨。**教训:Tab/Enter 这类键的含义取决于"焦点控件 + 它处于哪一级状态",框架给状态位和消费信号,具体行为归控件**。
- test_focus 验证:无焦点起步、TextInput 间 a→b→c 推进/回卷、隐藏跳过/焦点被隐藏回头、**SetFocus 落选择态、选择态 Tab 轮转不插空格、打字进编辑、编辑态 Tab 插空格不轮转、编辑态 Enter 结束编辑并轮转、EditView 选择态 Tab 轮转走/编辑态 Tab 插 4 空格**、全摘后不崩。**ctest 24/24**。

## 第 23 轮:EditView 128KB 容量(uint16→uint32 全 widen)+ 剪贴板 32KB + Dropdown 弹出框自适应宽 + EditView 配色/边框 API

txt_edit 实用化时用户报的两条 + 提的四需求推动的一轮库级改动(app 侧见 [[txt-editor-roadmap]] v4)。

**看不出编辑区范围 → 配色分层 + 边框 API**:根因是根容器底色(0x181820)与 EditView 底色(0x1C1C24)几乎同色,一眼分不清编辑区在哪。新增 `YMGUI_EditView_SetBgColor`(设 `d->bg` 同时设 `ev->bg_color`)与 `YMGUI_EditView_SetBorderColor`(1px 外框,alpha=0 则不画);evDrawCb 在恢复 clip 前用四条 `Draw_Fill` 描边。默认边框 0x505060。

**"只能存几行" → 定容 2048 升 128KB,并把内部计量全 widen**:`GY_EV_TEXT_MAX` 2048→131072。**这不是改个常量就完事——是一次类型安全改造**:EditView.c 里所有 len/cursor/line_count/line_cap/undo 长度/局部索引原为 uint16,在 128KB 下必坏:①SetText 的 `for(i=0;i<GY_EV_TEXT_MAX-1;i++)` 若 i 是 uint16,到 65535 自增回卷 → 死循环;②offset>65535 的选区/光标算术在 uint16 下截断包裹 → 坏堆。全部 widen 到 uint32。**坐标另说**:GYcoord 是 int16(max 32767),而 128KB 文本的像素总高/滚动偏移会超界,故 `scroll_y` 与 `contentH()` 用 int32(drag 起点像素仍 GYcoord,拖动量单独存 int32)。**API 一并 widen**:GetSelection/GetCursorRowCol 的 out 参从 `uint16*`→`uint32*`,GetLineCount/GetCursor 返回、ReplaceAll/GetSelectionText 计数改 uint32;调用点(test_editview.c、txt_edit updateStatus)同步。**Copy/Cut 不能放 128KB 栈临时**:新增 `copySelToClipboard` 就地把 `text[e]` 暂置 '\0' 借用起点指针给剪贴板,再还原,零大栈拷贝。

**剪贴板 32KB**:`GY_CLIP_MAX`(YMGUI_Hal.c)2048→32768,注释注明裸机可用编译宏改小省 RAM。库内静态兜底 buf 与它同量级。

**Dropdown 弹出框文字溢出 → 按内容自适应宽**:txt_edit 拿窄"菜单名"当触发框(90px),但选项"Find / Replace..."远长于 90px,原 openMenu `menu_w=ba.w` 致文字跑框外。改为遍历选项取 `max(ba.w, YMGUI_Font_TextWidth(opt)+OPT_TEXT_PAD*2)`(`OPT_TEXT_PAD 6` 对齐 optDrawCb 的 `abs->x+6`),再按 `disp->hor_res` 夹菜单 x 不越右沿(负则归 0)。菜单从此永远裹得住最长选项。

**验证**:clean build 0 警、ctest 24/24(test_editview 新增 100KB 存取不截断 + SelectAll 选区跨越 offset>65535 两条回归);txt_edit headless 落约定路径 `build/project_Demo/txt_edit/` exit 0,自检打印文件存取往返 OK。

## 累计状态（更新）

| 轮次 | 主题 | 测试数 |
|------|------|--------|
| 1 | 地基 | 1 |
| 2 | GUI 核心 | 2 |
| 3 | 多矩形脏区+字体+Label | 5 |
| 4 | 表单四件套 | 6 |
| 5 | 图元层 | 7 |
| 6 | 显示控件 | 8 |
| 7 | 粗弧+键盘+TextInput | 10 |
| 8 | 审计修复 | 11 |
| 9 | 滚动+List | 13 |
| 10 | Chart 折线图 | 14 |
| 11 | top_layer 弹出层 + Dropdown | 15 |
| 12 | Table 表格 | 16 |
| 13 | Tabview 标签页 | 17 |
| 14 | 状态/数据绑定地基 | 18 |
| 15 | 抗锯齿(可裁减) | 19 |
| 16 | 头文件拆分 + 3 处 AA bug 修复 | 19 |
| 17 | 中文/CJK 字体(UTF-8,可裁减) | 19 |
| 18 | 异步双缓冲(buf2 ping-pong + DMA 重叠) | 20 |
| 19 | 数据绑定铺到仪表类(Arc/Meter 单向) | 20 |
| 20 | 轻量布局助手(Stack + Align,可裁减) | 21 |
| 21 | 可滚动多行文本视图(TextView) | 22 |
| 22 | TextInput 中文输入修复 + 可编辑多行 EditView + GB2312 破折号补字 + Tab 焦点轮转 | 24 |
| 23 | EditView 128KB(uint16→uint32 widen)+ 剪贴板 32KB + Dropdown 弹出框自适应宽 + EditView 配色/边框 API | 24 |
| 24 | EditView 容量按需分配(创建期传参)+ 索引全 size_t + undo 可关(默认开) | 24 |
| 25 | TreeView 树形视图(自绘,展开/收起+懒加载,可裁减)+ files_manager demo | 25 |
| 26 | Grid 网格控件(自绘,二维滚动/单元格选中/编辑意图,可裁减)+ excel_edit 电子表格 demo | 26 |
| 27 | Grid 矩形选区+合并区+每格对齐 / excel_edit 插入行列+公式引用自动调整 | 26 |
| 28 | Grid 每行独立行高+插入删除行列 API / excel_edit 删行列+#REF! / 修合并选中吸附 | 26 |
| 29 | 修 excel_edit 列数 10→26(横滚生效)+ 订正独立工程构建/验证误报 | 26 |
| 30 | Canvas 可绘制位图视口 + ColorPicker HSV 取色(均可裁减)+ image_edit 类 PS 图层画板 | 28 |
| 31 | image_edit 形状实时预览 + 图层缩略图列表 + 增删/选中图层(纯 app 侧,库未动) | 28 |
| 32 | Roller 居中高亮平滑滚动列表 + Spectrum 多柱动画条(均可裁减)+ music_player 音乐播放器 | 30 |
| 33 | Spectrum 泛化重命名为 BarChart(配色 BY_HEIGHT/PER_BAR × 顶标 BAR/NONE)+ music_player 接真曲目默认输入 | 30 |
| 34 | BarChart BY_HEIGHT 改逐行竖直渐变 + Roller 高亮按离中线距离跨行渐变(消除滚动后跳行迟滞) | 30 |
| 35 | Button 支持按状态贴图(SetImage 图优先 + SetBgVisible 纯图标)+ music_player 播放/暂停改 ▶/⏸ 图标 | 30 |
| 39 | 跨平台 SDL/CMake 移植回收 + 右键/长按上下文输入与拖动 + Context Gesture Lab | 32 |

### 第 30 轮:Canvas + ColorPicker 两控件 + image_edit 类 PS 图层画板

第四个 project_Demo 催生的库缺口。用户要一个"类 PS 画板:支持图层、融合、选颜色涂鸦"。经确认三点范围:①图层/融合/合成**全放 app 侧**(库只出通用位图视口);②**新造 ColorPicker 控件**(第二个缺口);③工具集 = 直线/矩形、填充桶、吸管、自由笔刷+橡皮,外加用户点名的**柔和笔刷(边缘渐隐)**与**喷枪(粒度可选:越细→色越柔和低色度,固定喷口按粒度散布 + 边缘过渡拉低色度)**。

**缺口确认**:库原有 Image/GYimg 只读(`const GYpx*`),**没有可变像素画布**。对标 lv_canvas,立 `YMGUI_Canvas`——自持 `cw*ch` 行优先 `GYpx` 显示缓冲(`GetBuffer` 直写)、整数倍缩放(1..16)、平移/手型平移、屏↔画布坐标映射(负坐标 floor 除,越界不误判成第 0 像素)、绘制回调(DOWN/MOVE/UP + in_bounds)、`IsDrawing`(供喷枪逐帧驱动——指针按住不动时 MOVE 不触发,靠这个每帧补喷)。`YMGUI_ColorPicker`——HSV SV 方块 + 色相条,**整数 HSV↔RGB(无 libm/FPU)**,拖动即回调。两控件都 `type` 保持 `GY_OBJ_Base` 走自绘范式,各带 `YMGUI_CANVAS`/`YMGUI_COLORPICKER` 一键可裁(整文件 `#if` 包裹,关掉 0 悬空符号,已验证)。

**分层铁律**:Canvas 不认识"图层/融合",ColorPicker 不认识"当前墨色/工具"——与 Grid 不懂公式、TreeView 不懂文件系统一致。image_edit.c 里:每图层 = RGBA 直存(直 alpha 非预乘,独立可见位/不透明度/混合模式);合成 = 棋盘底 → 自底向上叠可见层(有效 alpha = 像素 alpha × 图层不透明度,先按混合模式算叠加色再线性插值),写 Canvas 缓冲(`g_dirty` 脏标记,变了才重合成)。混合模式正常/正片叠底/滤色/线性减淡(单通道整数)。

**工具**:硬笔刷(实心圆 cov=255)、橡皮(削 alpha)、**柔和笔刷**(cov 随距圆心线性衰减,整数 ceil(sqrt) 近似 → 边缘渐隐)、**喷枪**(固定喷口按 spray=rad*4 个散点落半径内,**单点 cov = 8 + (grain-1)*8**——粒度越细单点 cov 越低 → 直 alpha 累积越柔和低色度,越粗越实;xorshift32 确定性 PRNG 保 selftest 可复现)、直线/矩形(DOWN 记起点 UP 定形,Bresenham)、填充桶(4 邻域 flood fill 静态栈免递归,按图层色+alpha 容差 24 匹配)、吸管(从合成结果取色 → 设墨色 + 同步 ColorPicker)。拖动经 `strokeTo` 沿上次点补点(步长 ≈ rad/2)防断线。

**验证**:core clean build 0 警、ctest 28/28(新增 test_canvas:分辨率/GetBuffer 直写/屏↔画布映射含 zoom/pan/越界/负坐标/SetPan 钳制/绘制回调坐标+相位/平移模式拖动改 pan 不派绘制/IsDrawing;test_colorpicker:HSV↔RGB 纯色+round-trip/SetColor 不触发回调/点击 SV 角+色相条端触发回调)。image_edit 在自己 build 目录 0 警、headless `selftest: all tools + compositing OK` + exit 0(10 项:硬笔刷实心+圆外不溢/柔和中心>边缘/橡皮清 alpha/喷枪粗>细同种子/直线端点中点/矩形四角+内部空/填充铺满+上色/合成取色一致/半透明层向底靠拢/隐藏层不合成)。裁减验证:`-DYMGUI_CANVAS=0 -DYMGUI_COLORPICKER=0` 全库逐文件编译 0 错 0 警,两 .o `nm` 无 T/D/B 符号。**沿用第 29 轮教训**:app 在 `build/project_Demo/image_edit/` 单独重编再验。

### 第 31 轮:image_edit 形状实时预览 + 图层缩略图列表 + 增删/选中图层(纯 app 侧,库未动)

第 30 轮交付后用户四点反馈,全在 image_edit.c 内解决,**库一行未改**(印证 Canvas/ColorPicker 的通用面够用):

- **形状实时预览**:把形状栅格化与"落点动作"解耦——`rasLine/rasRect/rasTri/rasCircle/rasPoly` 都收一个 `PlotFn` 回调。`plotLayer` 盖实心圆到当前图层(提交),`plotPreview` 直接把墨色写进 Canvas 显示缓冲(预览)。拖动中每次 MOVE 先 `composite()` 重建底图再叠预览形状、**不置 `g_dirty`**(预览留屏到下次事件),UP 才真正提交。直线/矩形不再"UP 才见形"(第 30 轮的取舍取消)。
- **新工具:三角形/圆形/折线/多边形**。三角形取拖框上边中点为顶点+底边两角;圆形以起点为圆心、到当前点距离为半径(中点画圆 + 整数 `isqrt32` 免 FPU);折线/多边形逐次点击加顶点(`g_poly_x/y[64]`+`g_poly_n`)、橡皮筋预览随光标,**点回起点 8px 内且够点数**(折线≥2/多边形≥3)结束/闭合并提交,切工具丢弃未完成顶点。工具枚举新序:BRUSH/ERASER/SOFT/AIRBRUSH/LINE/RECT/TRI/CIRCLE/POLYLINE/POLYGON/FILL/PICK。
- **右侧图层缩略图列表**(app 侧自绘对象 `g_panel`:`YMGUI_Creat_Obj_Creat` + 直挂 `draw_cb`/`event_cb`,同 Grid/Canvas 自绘范式)。行序=PS 序(顶层在最上,`rowToLayer=count-1-row`);每行 = 降采样缩略图(`buildThumb`→`s_thumb[64*48]` 叠棋盘底,`YMGUI_Draw_Img` blit)+ 层号/混合/不透明度 + 右下眼睛显隐块。**点行选中该层 → `syncLayerControls()`**(把不透明度滑块/混合下拉[`Dropdown_SetSelected` 不触发回调]/标签/面板同步到该层);点眼睛切显隐(`GY_EVENT_Clicked` 命中,读 `ctx->point_x/y`-abs)。
- **增删图层 + 选中才改属性**:面板下方 ＋新建 / －删除(`onDeleteLayer` 上层结构体拷贝前移,末层不可删);所有属性(不透明度/混合/显隐/清空)都作用于面板选中的 `g_active` 层。布局改三栏:左控件 216 宽、中画布 x=232、右面板 `LP_X=802 LP_W=150 LP_ROW=54`。

**遗留清理**:上轮为省改动保留的两行 `if(dr<0)dr=-dr; if(dg<0)...`(floodFill + selftest)触发 `-Wmisleading-indentation`,本轮各 `if` 加花括号拆行,`image_edit.c` 在 `-Wall -Wextra` 下 0 告警(仅剩库头 `PubDefine.h` 的既有 multi-line comment,非本文件)。

**验证**:image_edit 自己 build 目录 clean build 0 警、headless `selftest: all tools + compositing OK` + `image_edit exit ok` exit 0,自检从 10 项扩到 **14 项**(加三角形顶点+底角/圆形边缘处被涂且圆心不描边/多边形三顶点闭合/增删图层[计数+内容前移+末层不可删])。库侧 ctest 28/28 无回归。**取舍**:形状预览每 MOVE 全画布 `composite()`(定容 320×240 开发机够快,真机可改局部脏区);折线/多边形靠"点回起点"闭合(Canvas 回调只有 DOWN/MOVE/UP,无双击/右键语义);图层缩略图列表是 app 侧自绘,若后续多项目复用可再抽库控件(延续暴露库缺口路线)。

### 第 29 轮：excel_edit 横滚 + 订正"跑旧二进制"误报

**现象**(用户报三连):①上一轮改动"感觉没变化";②插入列后网格不向右扩展、"像被固定了",右边还有空网格空间;③行号列(1/2/3)能上下拖看数据,但列名行(A/B/C)不能左右拖。

**根因一 = 跑了旧二进制(本轮最重要的教训)**:`project_Demo/excel_edit` 是**独立 CMake 工程**,有自己的构建目录 `build/project_Demo/excel_edit/`,顶层 `cmake --build build` **完全不编译它**(顶层 CMakeLists 不含 project_Demo)。上一轮(第 28 轮)我只 build 了顶层就验证,跑的其实是改动**之前**的旧可执行文件——所以用户看不到任何变化。**更严重**:第 28 轮我报告的 excel_edit "headless exit 0" 是**无效验证**,因为对的也是旧二进制。真正重编后,自检立刻爆 5 处失败——我第 28 轮写的删行/删列测试数据行号越界(用了 `commitCell(30/40,...)`,而 `G_ROWS=30` 合法行仅 0–29,`setCellRaw` 静默忽略→引用取空值→连锁崩)。已改到界内(25/26/27 行)重跑通过。**教训**:改独立工程的 app 必须 `cmake --build build/project_Demo/<name>` 或先看二进制时间戳确认已重链,否则 selftest 断言根本没执行到。

**根因二 = 内容装不满视口,横向无可滚空间**:原 `G_COLS=10`,内容宽 10×72=720px,而网格视口约 904px(`SCR_W-16` 减行号列 40)。720 < 904,右侧留白,`clampScroll` 把 `scroll_x` 钳死在 0,列名行拖了不动;插入列语义是"整体右移、末列挤出、列数不变",内容又装不满,视觉上毫无变化。真 Excel 的"右扩感"也来自列多到需要横滚。**修**:`G_COLS 10→26`(用满库上限 A–Z),内容宽 1872px > 视口,列名行拖拽平移生效、插入列有"右推"效果。表头拖拽平移逻辑本就正确(同改 scroll_x/scroll_y),纯粹之前无内容可滚。行号列能上下拖是因 30×22=660px > 视口高,纵向本就有得滚。

**验证**:核心顶层 build 0 警 + ctest 26/26;excel_edit **在其独立构建目录**重编 0 警 + headless exit 0(删行删列引用→#REF! 断言这次真跑通)。

### 第 28 轮：Grid 每行独立行高 + 插入/删除行列 + 合并选中吸附(修 5 处反馈)

**动机**:用户试用 excel_edit 后报 5 个问题 + 加删行列。诊断后归为 4 类修复。

**改动(库侧 Grid)**:
- **合并后文字消失 & 看不出选中(同一根因)**:单击落在合并区**被覆盖子格**而非锚点→输入写进隐藏格、高亮判定落空。加 `snapToAnchor`(命中合并区→吸附到 r0,c0)用于单击/双击;三趟绘制里合并区高亮改**矩形相交**判定(`m->r0<=sr1 && m->r1>=sr0 && ...`)而非只看锚点是否被选中。
- **行高+改了所有行**:根因 app 调的是全局 `SetRowHeight`。Grid 加**每行独立行高** `row_hs[GY_GRID_MAX_ROWS]`;新增 helper `rowH/rowTop/rowAtY`,`contentH`/body 绘制/行号列/`cellAtPointer`/`cellAtPointerClamp`/`EnsureVisible`/`GetCellRect` 全改吃 `row_hs`;`SetRowHeight` 语义=全局铺到每行;新 API `SetRowHeightAt/GetRowHeightAt`;新 API `GetColWidth`(app 遂删 `g_col_w[]/g_row_h` 影子数组,库为唯一真源)。
- **插行后合并不动**:Grid 加 `InsertRow/DeleteRow/InsertCol/DeleteCol`——只搬库自身拥有的(文字 `copyCell/clearCell`、对齐、行高列宽、合并区)。合并区规则:跨插入点增大 / 其后整体移位;整片落被删行列丢弃 / 跨删除线缩小;`dropDegenerateMerges` 清退化(单格/越界);选区一律清空(交 app 重设)。

**改动(app excel_edit)**:`deleteRowAt/deleteColAt`(g_cells 上移/左移,末行列清空)+ `rewriteFormulaRefsDelete`(引用**正落被删行/列 → `#REF!`**,更后引用 -1);parser 的 `parsePrimary` 认 `#` 开头错误 token(吞整段置 `ERR_REF`);第二排工具栏加"删行/删列"按钮;行高±改成只调**活动行** `SetRowHeightAt(g_sel_r,...)`。

**验证**(注:本轮 excel_edit 侧实际未真编译,见第 29 轮订正):test_grid 加 getter/单行行高不牵连/插入删除合并移动/删行整片消失/选区清空/越界忽略覆盖。

### 第 26–27 轮：Grid 网格控件 + excel_edit 电子表格

**第 26 轮 起 Grid + excel_edit**:第三个 project_Demo(电子表格)催生的库缺口。**分层铁律**:库侧 Grid 只管显示串/单元格选中/二维滚动/编辑意图,**不认识公式**;电子表格语义(A1 地址、公式引擎、重算、定点格式化)全在 `excel_edit.c`。Grid 仿 Table/TextView/TreeView 的自绘范式(单 draw_cb + user_data,`type` 保持 `GY_OBJ_Base`);sticky 顶部列名行(A/B/C)+ 左侧行号列(1/2/3)+ 左上角块;二维滚动、列宽各异;扁平单元格缓冲 `cells[(row*col_cap+col)*CELL_LEN]` 容量创建期传参(学 EditView 教训,不用大默认绑架 RAM)。公式引擎纯 app 侧递归下降(`expr/term/factor/primary`,Parser 结构体穿参非全局游标),定点 16.16,SUM/AVG/MIN/MAX,`visiting` 标志查循环引用、`computed` 记忆化,`#DIV0!/#REF!/#ERR!` 错误码,全表重算(网格小,拓扑排序是过度设计)。编辑两路:单击→公式栏、双击→就地浮层 TextInput(`GetCellRect` 定位),逐键实时提交。

**第 27 轮 加四功能**(用户点名:插入单元格/合并/间距/居中):
- **插入行/列 + 公式引用自动调整**:app `insertRowAt/insertColAt` 移位 g_cells + `rewriteFormulaRefs` 扫每个公式 raw 里地址 token(自解析字母段+数字段,函数名原样留),行≥插入行 +1、列≥插入列 +1,覆盖 `SUM(A1:B5)` 两端点。
- **Grid 选区升级为矩形区**:`sel_row/col`=活动格 + 新增 `anchor_row/col`。API `SetSelectedRange/GetSelectedRange`(归一);`GetSelected` 仍返活动格(向后兼容)。**手势冲突解**(无滚轮事件,拖拽是唯一滚动手势):单元格区起手拖=框选(活动格跟指针钳位+自动滚),表头起手拖=平移滚动(`pointInBody` 分流);Shift+方向扩选。
- **Grid 合并区**(纯视觉):定长 `merges[16]`。API `MergeCells`(归一/重叠拒/单格拒/满额拒)、`UnmergeAt`、`GetMergeAt`。绘制改三趟(非合并格+文字 / 列竖线 / 合并区整片+外框+文字);命中/`GetCellRect` 落合并区→归一到锚点+整片尺寸。
- **Grid 每格对齐**:`uint8* align` 缓冲(默认左),`SetCellAlign/GetCellAlign`,`drawCellText` 按对齐算 x。app 加第二排工具栏(插入行列/合并/取消/左中右/列宽±/行高±)。

### 第 25 轮：TreeView 树形视图 + 文件管理器 demo

**动机**：起 `project_Demo/files_manager`(第二个"真小应用"验证项目)时,需要 VS Code 式的文件树——文件夹能就地展开/收起看下一级。先评估复用 List:List 的每个条目是常驻 GYOBJ 子对象,没有缩进/标记/懒加载,展开/收起要频繁增删子对象(重建即泄漏风险),且无层级概念。判定这是**真实库缺口**,新起 TreeView 控件而非硬改 List。

**TreeView 设计**(仿 Table/TextView 的自绘型):
- **节点树 + 展平可见数组**:节点是轻量 `GYtree_node`(非 GYOBJ),`child_head/child_tail/sibling` 链表建树;控件持一个指针数组 `vis[]`,展开/收起时 DFS 重建(只收展开路径上的节点)。draw_cb 只画可见行(首行=scroll_y/row_h),`type` 保持 `GY_OBJ_Base`,靠 user_data+draw_cb 区分——与 Table/TextView/EditView 同一自绘范式。
- **懒加载**:首次展开目录触发 `expand_cb`,回调里 readdir 填子节点;`loaded` 标志在调 expand_cb **之前**置 1,防重入(收起再展开不重复触发)。`ClearChildren` 复位 loaded 允许重新懒加载,并清选区防悬空指针。
- **交互**:单击标记(▶/▼)切展开、单击名字选中(select_cb);双击目录=切展开、双击文件=activate_cb;拖动超阈值(4px)即纵向滚动(否则算点击)。目录名暖黄 `0xF0C860`、文件浅灰。
- **可裁减**:编译期总开关 `YMGUI_TREEVIEW`,整控件 `#if` 包裹,`=0` 时裁空且库核心不依赖它 → 无悬空符号。

**files_manager demo**(960x600,左树右预览 + 顶部工具栏 + 底部状态栏):
- 浏览导航(懒加载真实文件系统,POSIX opendir/readdir/stat)、文本预览(.txt/.md/.c/.h 等,≤64KB)、新建文件夹/重命名、删除。
- **安全设计(保留)**:全部操作限定在 `$TMPDIR/files_manager_sandbox` 内;删除只 `remove`(单文件)/ `rmdir`(空目录),**不递归 rm**;重命名/新建名字拒绝 `/` 和 `..`。
- 复用 txt_edit 的 GB2312 外部字模回退(`Demo/gb2312_glyphs.bin`),中文路径/文件名可显示。

**验证**:核心 clean build 0 警、ctest 25/25(新增 test_treeview:建树/深度、展开收起重建可见数组、懒加载只触发一次、文件不可展开、user_ptr、单击选中+回调、标记单击只切展开不选中、双击文件 activate / 双击目录展开、滚动钳位、ClearChildren 减可见+清选区+可重新懒加载、Clear、析构);新增 demo_treeview(懒加载虚拟文件树 + 单击选中/双击打开 + 拖动滚动,headless exit 0)→ 18 个 demo;files_manager clean build 0 警 + headless exit 0 打印 "selftest: file ops + tree load OK";`-DYMGUI_TREEVIEW=0` 裁减 build 0 警 25/25(证实无悬空符号)。

### 第 24 轮：EditView 容量按需 + size_t 索引 + undo 可关

**动机**：裸机的 RAM 不该被库的"大默认值"绑架。原 EditView 把 `text[131072]` 和 `undo_text[131072]` 两块 128KB 数组硬编进控件结构体，无论调用方是要做密码框(几十字节)还是文本编辑器,都得先交 256KB "入场费"。这在 PC 上无感,在 MCU 上是致命的。

**改动**：
- **容量创建期传参**：`YMGUI_Creat_EditView_Creat(parent, x,y,w,h, size_t capacity)`。`text` 缓冲改为 `GY_malloc1(capacity+1)` 堆分配,`evFreeCb` 负责 `GY_free1`。调用方按实际需求申报(密码框传几十,编辑器传 `GY_EV_TEXT_MAX`)。`GY_EV_TEXT_MAX=131072` 从"控件内部固定尺寸"降级为"给要大编辑器的调用方的建议常量"。
- **索引全 size_t**：len/cursor/capacity/行表 off/len/line_count 等全部 uint32→size_t,便于将来往超 4GB 扩展;-1 哨兵字段(sel_anchor/undo_anchor、findFrom 返回值)用 ptrdiff_t(size_t 的有符号对偶)。像素字段(scroll_y/drag_start_scr)保留 int32(可为负、受屏高界定)。
- **undo 可关**：`YMGUI_EditView_SetUndoEnabled(ev, on)`。默认开,`undo_text` 也占一份 `capacity+1`;关掉即 `GY_free1` 掉那份镜像,省一半 RAM。undoSnapshot/undoRestore 都对 `undo_text==NULL` 做了短路。

**验证**：clean build 0 警,ctest 24/24(含新增 SetUndoEnabled 回归:默认撤销能还原 → 关闭后 Undo 空操作 → 重新开启再还原),txt_edit headless exit 0。

### 修复：ASCII 下划线 `_` 不显示(字模越格)

**现象**：文本编辑器里英文下划线 `_` 打不出来(其余字符正常)。

**根因**：`gen_font.py::emit_ascii` 以 `PT=15, Y_OFF=-1` 在 16px 高字格里渲染 DejaVuSansMono。`_` 的墨迹落在基线**下方**,比 `g/j/p/q/y` 这些降部字符还低一行——在这个偏移下整个字形掉出了 16px 格底,于是 `_` 的 64 字节字模全是 `0x00`(空)。渲染路径没问题(探针实测:单独画 `_` 第 15 行本该有 8px),纯粹是生成期把它裁没了。

**修复取舍**：先试过全局 `Y_OFF=-1→-2` 把 `_` 拉回格内,能显示但**英文整体下移一格,与 CJK 字体(单独 `Y_OFF=-5`)的顶部基线错开,中英混排看着别扭**(用户报)。改为**逐字形偏移例外**:全局保持 `-1` 不动,只给 `_` 单独用 `-2`(`GLYPH_Y_OFF = {ord('_'): -2}`),拉回第 15 行(下划线本该在的位置)。其余 94 个 ASCII 字模一字节未动,中英基线对齐不变。

**教训**:等宽字体的下划线/某些符号墨迹会低于降部基线,固定格高渲染时按整体偏移对齐会把它裁到格外。**对付个别越格字形,用逐字形 Y 偏移例外表,而不是动全局偏移**(动全局会连累中英基线对齐)。同类基线几何坑:第 17 轮 CJK 的 `PT=16/Y_OFF=-5`。

**验证**:`_` 字模末行恢复 `0xFF 0xFF 0xFF 0xFF`、`g` 等未变;core demo + txt_edit 全量重建 0 警,ctest 24/24。

### 第 32 轮:Roller 平滑滚动列表 + Spectrum 多柱动画条 + music_player 音乐播放器

第五个 project_Demo 催生的库缺口。用户要"音乐播放器:暂停/播放、进度条拖动、歌词显示,能开频谱更好"。确认三点范围:①音频用 **ffmpeg**(库核心是 GUI 不做音视频分析,只在实践中发现问题);②真声卡输出走 **SDL2,但 SDL2 音频部分必须留在 demo 侧不污染 GUI 库**;③新控件**不做"歌词专用"**——用户明确要"一个能复用的控件(支持平滑滚动、中间行高亮),不仅能显示歌词,还能显示时间/日历/拨码,再在其上动态显示歌词(可选最大显示几行、第几行高亮);频谱同理造一个能复用的"。

**两个通用控件(而非 LyricView/SpectrumView)**:
- **Roller** —— 居中高亮平滑滚动列表。选中行永远绘制在控件竖直正中并高亮,其余行按 `(i-cur)*row_h` 上下排开、越界裁掉;当前滚动位置 `cur` 是 **16.16 定点**,每帧 `Tick` 向"目标行"缓动逼近(`cur += (target-cur)/ease_div`,带最小步长防卡住)→ 平滑滚动。**两用互不排斥**:程序态(只 `SetSelected(idx)` 改目标,Tick 驱动动画——歌词/时间/日历用这个)、交互态(`SetInteractive(1)` 开启:拖动改 `cur`,抬起吸附到最近行并触发 `changed`)。行文本深拷进控件内部(定长 `GY_ROLLER_LINE_MAX=96` 字节/行),**库不认识"歌词"语义**。
- **Spectrum** —— 通用多柱动画条。N 个竖直柱,每柱按 `value/[0,max]` 映射高度;柱身按高度从 lo 色**线性插值**到 hi 色(整数);**峰值保持**:每柱记一个峰值标记,新值更高顶上去,否则每帧 `Tick` 衰减回落。频谱/直方图/电平表通用,**值由外部(app 侧 DFT)喂进来**,库不做频段分析。
- 两控件都 `type` 保持 `GY_OBJ_Base` 走自绘范式(单 draw_cb + user_data),各带 `YMGUI_ROLLER`/`YMGUI_SPECTRUM` 一键可裁(整文件 `#if` 包裹,关掉 0 悬空符号,已 nm 验证)。

**分层铁律**:音乐语义**全在 app 侧**(`project_Demo/music_player/`),与 Grid 不懂公式、Canvas 不懂图层一致:
- **mp_audio**(ffmpeg + SDL2,音频只出现在这一个文件)—— 无 ffmpeg 开发库,走 **popen ffmpeg CLI** 整曲解码成 44100/立体声/s16le 裸流存内存(`ffmpeg -nostdin -v error -i '<path>' -f s16le -ac 2 -ar 44100 -`,路径单引号转义防注入)。整曲进内存 → seek/进度/频谱都在一块缓冲上做,seek 即改帧游标 O(1)。输出用 **SDL_QueueAudio**(queue 模式非 callback),每帧维持约 0.3s 队列水位;可闻位置 = 已推送帧数 − 声卡队列剩余(`SDL_GetQueuedAudioSize`)。**开设备失败(headless/无声卡)→ 静音兜底**,位置改由 wall-clock(`SDL_GetTicks`)推进,UI 逻辑不变。SDL_LCD 只 init 了 VIDEO,mp_audio 自己 `SDL_InitSubSystem(AUDIO)`(销毁时 QuitSubSystem,且在 SDL_LCD_Destroy 之前)。**无文件/解码失败/ffmpeg 不可用 → 合成兜底旋律**(C 大调音阶琶音,库 Q15 `GY_Sin` + 三角窗包络 + 低八度叠加,纯整数)。
- **mp_lrc** —— `.lrc` 解析(`[mm:ss.xx]` 时间标签,一行多标签共享文本,按时间排序插入,二分查"播放毫秒→高亮行索引")。
- **mp_spectrum** —— 整数 **Goertzel** 频段分析(无 libm)。中心频率二次分布近似对数(低频密高频疏),coeff=`2*cos(w)` 用库 Q15 三角表,幅度用整数 `isqrt64`,高频段增益补偿。`uint64` 库 PubType 无此 typedef,本文件 `#include <stdint.h>` + `typedef uint64_t uint64` 自补。

**UI**(800×480):上方 Spectrum(24 柱)、中部 Roller 歌词(5 可见行,按时钟 `SetSelected` 到当前行平滑滚)、下方可拖动进度 Slider(range 0..1000,`changed` 回调换算毫秒 seek;拖动中靠 `slider->state & GY_STATE_Pressed` 判定不回写防抖)+ 当前/总时长标签 + 播放/暂停按钮。GB2312 全字库回退(复用 `Demo/gb2312_glyphs.bin`)。`argv[1]=帧数`、`argv[2]=音频文件`、`argv[3]=.lrc`;无 .lrc 时用内置默认词演示滚动。

**验证**:core clean build 0 警、**ctest 30/30**(新增 test_roller:行增删/选中钳制/可见行/立即vs缓动 Tick 收敛/交互拖动吸附/changed 回调/析构;test_spectrum:柱数钳制/SetValue+SetValues/range 映射/峰值保持抬升+回落/衰减 Tick/颜色渐变/析构)。新增 demo_roller + demo_spectrum(headless exit 0)→ 23 demo。music_player 在自己 build 目录 0 警、headless `selftest: lrc + spectrum + audio engine OK` + `music_player exit ok` exit 0(3 组:LRC 解析排序+查询、频谱正弦有能量/静音全 0、音频引擎合成兜底+seek 钳制+位置+peek);实测 ffmpeg 解码真实 wav(264600 帧/6000ms 正确载入)+ 静音兜底两路都通。裁减验证:`-DYMGUI_ROLLER=0 -DYMGUI_SPECTRUM=0` 两 .o `nm` 无 T/D/B 符号。**沿用独立工程验证坑**:music_player 在 `build/project_Demo/music_player/` 单独 configure+重编再验(顶层 build 不含它)。**取舍**:整曲解码进内存(3 分钟立体声 ≈ 31MB,开发机够用,真机可改流式);频谱是整数 Goertzel 近似(能动、跟音乐相关,不追真 FFT 精度——定调库核心是 GUI);进度条拖动检测借 `state` 位(无专门"拖动开始/结束"事件)。

### 第 33 轮:Spectrum 泛化重命名为 BarChart(通用柱状图)+ music_player 接真曲目

用户放了一首真曲目(`可能-队长.mp3` + `.lrc`)进工程,并对上一轮的 Spectrum 提了两点:①这控件除了频谱还能干别的吗?②能不能加开关让它变普通柱状图?——用户点明**本意就是一个普通柱状图**(所以不建议叫 Spectrum),外加**顶部回落**(可文字可高亮条),且柱色要两种模式:**每柱独立颜色** / **所有柱同色但不同高度不同色**。

**回应**:Spectrum 本就是"库不认识音乐"的通用多柱条(频谱只是喂它的一种数据),但名字误导。经确认(AskUserQuestion)四点:控件名 **BarChart**;顶部回落库侧**只做 NONE/BAR 两模式**(文字顶标交 app 叠 Label,库不引 Font 依赖);**彻底替换 Spectrum 不留别名**;两种配色都要。

**改动**:`YMGUI_Spectrum.{h,c}` → `YMGUI_BarChart.{h,c}`(删旧文件),裁剪宏 `YMGUI_SPECTRUM` → `YMGUI_BARCHART`。两个正交维度:
- **配色 ColorMode**:`BY_HEIGHT`(默认,柱色随高度在 lo→hi 间整数插值——同一物理量不同强度,频谱/电平表)/ `PER_BAR`(每柱一个调色板色,与高度无关——不同类目统计)。`SetGradient` 设渐变端点;`SetBarColor`/`SetBarColors` 设调色板;`SetBarCount`/`SetGradient` 时按 lo→hi 给调色板铺一版默认渐变,PER_BAR 未显式设色也有合理默认。
- **顶部回落 TopMode**:`TOP_BAR`(默认,高亮细条被新值顶起、每帧 `top_fall` 回落——峰值悬停手感)/ `TOP_NONE`(纯柱)。`SetDecay(0,0)` + `TOP_NONE` = 一张静态普通柱状图,无需额外开关。
- 拆分旧 `SetColors(lo,hi,peak,bg)` → `SetGradient(lo,hi)` + `SetTopColor` + `SetBgColor`;旧 `SetPeakHold(on)` 并入 `SetTopMode`。

**music_player 接真曲目**:CMake 把工程自带的 `可能-队长.mp3`/`.lrc` 绝对路径经 `MP_DEFAULT_AUDIO`/`MP_DEFAULT_LRC` 传进去,`main` 无 argv[2]/[3] 时默认放这首(.c 里有 `NULL` 兜底 define,脱离 CMake 也能编)。频谱面板改用 BarChart 配 `BY_HEIGHT + TOP_BAR`。

**验证**:clean build 0 警、**ctest 30/30**(test_spectrum → test_barchart:柱数/值钳制、顶标保持、Tick 回落、**TopMode NONE 高处无顶标像素 vs BAR 有**、**配色 PER_BAR 各柱独立设色+切换重绘**、渲染高柱像素多于矮柱、析构);demo_spectrum → demo_barchart(四组合 BY_HEIGHT/PER_BAR × BAR/NONE 每 ~3s 轮换,headless exit 0)。music_player 无参默认加载 `可能-队长.mp3`(**10095200 帧 / 228916ms ≈ 3:49 正确载入**)、headless selftest exit 0。裁减验证:`-DYMGUI_BARCHART=0` 该 .o `nm` 无 T/D/B 符号。全仓 `grep` 无残留 `YMGUI_Spectrum`/`YMGUI_SPECTRUM`(app 侧频段分析器 `mp_spectrum` 名字正确,保留)。

### 第 34 轮:BarChart 竖直渐变 + Roller 高亮跨行渐变(两处观感 polish)

实测真曲目后用户提了两处观感问题:①BarChart `BY_HEIGHT` 是**整根柱一个色随高度变**,希望**柱色沿高度渐变**;②Roller 歌词**滚动完要过一会儿高亮才跳下一行**,期望滚动完立即切换或**边滚边过渡**(高亮随行位置上下淡入淡出)。

**BarChart**:`BY_HEIGHT` 从"整柱单色(按柱高取一个插值色)"改成**逐行竖直渐变**——每根柱自底 `c_lo` 向上逐像素行插值到 `c_hi`,以控件全高为标尺(柱越高其顶端越接近 `c_hi`)。`PER_BAR` 不变。新增 `drawBar()`(PER_BAR 走整柱单填,BY_HEIGHT 走 1px 行循环 `lerpColor`)。

**Roller**:根因是绘制里 `center_i = floor(cur)` 且 `i == center_i` 全有或全无地取高亮色——滚动缓动 `cur:3.0→4.0` 期间 `floor` 一直是 3,直到贴到 4.0 才跳,于是高亮"黏"在旧行末尾才跳。改为**按行到正中线的定点距离 `|i-cur|` 在 `c_normal↔c_hi` 间线性插值**:恰在中线=全高亮,±1 行外=全普通,中间过渡;`center_i` 也改 `round(cur)` 让可见窗口对称。静止时(`cur` 落在整数行)与旧观感完全一致(中心行全亮、余行普通),无回归。复用 BarChart 同款 `lerpChan`/`lerpColor`(各自文件内静态,未提公共层)。

**验证**:clean build 0 警、**ctest 30/30**;test_barchart 增"同柱顶/底像素色不同(竖直渐变)"断言过;music_player 重编 0 警。Roller 静止态旧断言仍过(证明无回归)。

### 第 35 轮:Button 按状态贴图 + music_player 图标按钮

用户问"现在的 Button 能贴图吗、不同状态(如播放/暂停)用贴图代替文字"。现状:Button 只画纯色底+边框+居中文字,无图片字段。但库已有 `GYimg`+`YMGUI_Draw_Img`(Image 控件在用)——接进来即可,不造新图元。

**AskUserQuestion 定三点**:底色/边框**给开关自选**;**只一张图,状态由 app 换**(不做 normal/pressed 双图);**图优先**(有图不画文字)。

**改动**:
- `YMGUI_Button.{h,c}`:私有数据加 `GYIMG src`(默认 NULL)+ `uint8 draw_bg`(默认 1)。`btnDrawCb` 把底色+4 边框包进 `if(draw_bg)`;有图(`src && src->data`)→ 居中 blit(照抄 Image 控件居中算式)、**不画文字**;无图→走原居中文字。新增 `YMGUI_Button_SetImage(btn, src)`(NULL=清图回退文字)/`SetBgVisible(btn, on)`。`#include YMGUI_DrawImg.h` 拿 `GYIMG`。
- **向后兼容**:不设图=老行为,所有现存 Button 调用零改动。图不拥有像素(同 Image,调用方保证存活,裸机多是 const flash 图);异形图标靠 colorkey。
- **music_player 落地**:app 侧程序生成 ▶/⏸ 两张 28×28 图标(colorkey 品红透明抠形),`g_btn_play` 从 `SetText("播放"/"暂停")` 改 `SetImage`,新增 `updatePlayIcon()`(播放中显 ⏸ 否则 ▶)统一在 toggle/开播/播完三处调。

**验证**:clean build 0 警、**ctest 30/30**(test_display 加 Button 贴图断言:图 blit 出≥64 红像素、`SetBgVisible(0)` 后图仍在、`SetImage(NULL)` 回退文字且无红像素);demo_button 补纯图标 ▶/⏸ 切换按钮 headless exit 0;music_player 重建 0 警、headless selftest exit 0。**教训:Button 贴图不必造新机制,复用 GYimg+Draw_Img 图元 + 图优先分支即可;状态切换归 app 换图,和 SetText 换字同一套路**。

### 第 39 轮：跨平台 SDL 移植回收 + 上下文输入/拖动 + Context Gesture Lab

从 `/home/yaomi/ClashCore/YMGUI` 的 Windows/Android 移植实践反向筛选通用改动。核心 `YMGUI/` 与原仓内容一致，故**不覆盖控件/图元代码，也不合入 ymproxy/Clash 业务目录**；只回收平台桥接和构建层。

**跨平台 SDL/CMake**：`SDL_LCD_Init` 改返回 `int`，逐级检查 SDL/Window/Renderer/Texture/截图缓冲；硬件 renderer 失败回退 software，失败路径统一释放资源，Destroy 清空静态句柄。补 SDL finger 单指映射与 `SDL_TOUCH_MOUSEID` 去重。`project_Demo/ymgui_app.cmake` 保持 Linux pkg-config，同时支持 Windows/Android 的 SDL2 CMake target、Android shared library、Windows `SDL2main`/可选静态 SDL2，且 Windows 不链接 `libm`。新增 `CROSS_PLATFORM_PORTING.md`，明确平台逻辑只放 SDL_LCD/应用桥接层。

**上下文语义**：没有把右键伪装成普通 Clicked，而是新增平台无关事件 `ContextRequested/ContextDragging/ContextReleased/ContextCancelled`。右键短点击是一发 one-shot Request；右键移动超过 4 个逻辑像素、或触摸保持 600ms 且移动不超过 10px 后激活捕获式生命周期 `Requested → Dragging* → Released/Cancelled`。`GYctx.context_obj` 与普通 `pressed_obj` 独立，不改焦点、不置 Pressed、不产生普通 Clicked；对象中途释放会清捕获。新增 `PointerCancel`，以 ReleasedOff 结束普通按压，取代伪造 `(0,0)` 抬起。长按超时用无符号 tick 差保证 32 位回绕安全，静止时由每轮 PumpEvents 末尾检查；切后台、失焦、退出统一取消。

**可视化验证**：新增 `project_Demo/context_gesture`（Context Gesture Lab，480×272，不新增控件）：三张由基础 `GYOBJ` + app 自定义绘制组成的可拖彩色卡片、实时状态/捕获/坐标和合并后的事件流；右键短点/长按不动弹 top_layer 菜单，右键拖动/长按后拖动移动卡片，Cancel 恢复本次手势起点。`YMGUI_SHOT` 自动构造 A=Release、B=Cancel、C=菜单场景，正式图在 `docs/shots/context_gesture.png`。

**验证**：clean build 全目标成功；扩展 `test_event` 覆盖核心捕获/幂等/对象释放安全；新增 SDL dummy + `SDL_PushEvent` 的 `test_sdl_context` 覆盖右键短点、4px 阈值、拖动顺序、失焦取消和长按后 Move/End。最终 **ctest 32/32**，`project_Demo/context_gesture` 与 `demo_flush_band` headless exit 0，`git diff --check` 通过。Windows/Android 真机工具链本机未具备，CMake 分支与 SDL 事件映射已静态检查，不能冒充实机验证。

### 第 40 轮：插件注册表 Demo + 生命周期状态机

新增 `YMGUI/PLUGIN` 轻量插件层与单文件 `Demo/demo_plugin.c`。插件支持版本校验、静态注册、`init/tick/deinit` 生命周期；Unix 桌面另提供可选 `dlopen` 入口。注册表内部补齐 Registered/Active/Failed 状态：重复 `LoadAll` 不会二次初始化，失败初始化立即走 `deinit` 回滚且不隐式重试，`UnloadAll` 后静态插件可重新加载，动态插件 `dlclose` 后从表中移除以避免悬空描述符，`Reset` 会先安全卸载再清表。新增 `test_plugin` 覆盖无效描述符、重复 ID、版本不匹配、容量上限、失败回滚、tick 隔离、卸载重载和活跃态 Reset。 随后拆分平台无关注册表与 OS 动态加载器，新增 `struct_size` ABI 校验、`YMGUI_PLUGIN_EXPORT` 和 `YMGUI_PLUGIN_DYNAMIC` 裁剪开关；`project_Demo/plugin_host` 实际验证 Linux `.so` 两轮加载/卸载/重载，并用 LLVM-MinGW 生成 Windows x64 `.dll/.exe`、用 NDK 生成 Android arm64 两个 `.so`，导出符号均已检查。 用户指出原 `plugin_host` 只有命令行验证、不算项目后，将其升级为 800x480 插件管理器：左侧插件清单/状态，顶部 Load/Unload，底部生命周期日志；动态 `system_info` 插件通过扩展后的宿主函数表自行创建指标卡、运行时间和 Refresh 按钮。CTest 用指针注入真实执行 Unload→Load→插件按钮，正式截图落 `docs/shots/plugin_host.png`。Windows UI EXE+DLL 与 Android arm64 SDL UI 宿主+插件 SO 均交叉构建通过。

当前：26 控件（含 base）、5 类图元、33 单测全过、25 个 demo（+10 个 project_Demo 完整应用：txt_edit、files_manager、excel_edit、image_edit、music_player、video_player、dashboard、alarm_clock、context_gesture、plugin_host），clean build 0 错 0 警。EditView 容量创建期按需申报(text 堆分配,索引全 size_t,undo 默认开可关省 RAM),剪贴板 GY_CLIP_MAX 32KB,可设底色/边框;Dropdown 弹出框按最长选项自适应宽并夹屏内。Tab 焦点轮转(前序遍历、跳过 Hidden、末尾回卷,`YMGUI_FocusNext`)已落地。抗锯齿(AA)已落地:混合基石 GY_MixPx/GY_BlendPx(DrawPx.h 共享)+ Wu 斜线 + 圆/弧/实心圆 coverage(填充/粗描边走**距离场**)+ 4bpp 灰度字体,`YMGUI_ANTIALIAS` 一键可裁(默认开,单色屏强制关)。中文字体(UTF-8 回退链 + 稀疏字模 + 外部 flash 回调 + 字模来源可选)已落地,`YMGUI_FONT_CJK` 一键可裁。异步双缓冲(buf2 ping-pong,运行期字段可选)已落地。数据绑定覆盖 8 控件(标量双向 + app 订阅)。轻量布局助手(Stack/Align 一次性,`YMGUI_LAYOUT` 可裁)已落地。多行文本视图 TextView(只读可滚,Multiline/Wrap 两可选属性,仿 Table 自绘)已落地,附带 CORE 层按字节长文本图元 Draw_TextN/TextWidthN。树形视图 TreeView(节点树+展平可见数组,展开/收起+懒加载+缩进标记,仿 Table 自绘,`YMGUI_TREEVIEW` 一键可裁)已落地,由第二个 project_Demo 验证项目 files_manager(沙箱文件管理器)催生。网格控件 Grid(自绘,二维滚动+单元格/矩形选区+合并区+每格对齐+每行独立行高+插入删除行列,`YMGUI_GRID` 一键可裁)已落地,由第三个 project_Demo 验证项目 excel_edit(电子表格:纯 app 侧递归下降公式引擎+定点+全表重算)催生。可绘制位图视口 Canvas(自持 GYpx 显示缓冲直写+整数倍缩放+平移+屏↔画布映射+绘制回调+IsDrawing 喷枪逐帧,`YMGUI_CANVAS` 一键可裁)与 HSV 取色器 ColorPicker(SV 方块+色相条,整数 HSV↔RGB 无 FPU,`YMGUI_COLORPICKER` 一键可裁)已落地,由第四个 project_Demo 验证项目 image_edit(类 PS 图层画板:图层栈/融合/合成+硬/柔和/喷枪笔刷+橡皮+直线/矩形/三角/圆/折线/多边形[拖动实时预览,形状栅格化解耦 PlotFn]/填充桶/吸管+右侧图层缩略图列表[app 侧自绘,点选切层/眼睛显隐/增删层]全在 app 侧)催生。居中高亮平滑滚动列表 Roller(选中行居中高亮[按离正中线距离在普通↔高亮色间跨行渐变,滚动时高亮随内容平滑跟随不迟滞]+其余行上下排开越界裁+16.16 定点位置每帧缓动逼近目标行,程序态[SetSelected 驱动,歌词/时间/日历用]+交互态[拖动滚动抬起吸附]两用,`YMGUI_ROLLER` 一键可裁)与通用柱状图 BarChart(N 柱按值映射高度,配色 BY_HEIGHT[柱身底 lo→顶 hi 逐行竖直渐变]/PER_BAR[每柱独立调色板] × 顶标 BAR[峰值保持每帧回落]/NONE[纯柱],SetDecay(0,0)+NONE 即静态普通柱图,频谱/直方图/电平表/统计柱图通用,值由 app 喂,文字顶标交 app 叠 Label 库不引 Font,`YMGUI_BARCHART` 一键可裁)已落地,由第五个 project_Demo 验证项目 music_player(音乐播放器:ffmpeg popen 整曲解码+SDL2 声卡 SDL_QueueAudio+静音兜底+合成旋律兜底+.lrc 解析+整数 Goertzel 频段分析全在 app 侧,SDL2 音频只在 mp_audio 一个文件不进库;第 33 轮起默认加载工程自带真曲目)催生。原名 Spectrum 于第 33 轮泛化重命名为 BarChart。**独立工程验证坑**:project_Demo 各 app 有独立构建目录 `build/project_Demo/<name>/`,顶层 build 不含它们,改 app 必须单独重编再验证(第 29 轮踩过误报)。

## 待办

- **三个弹出层/显隐地基相关控件(Dropdown/Table/Tabview)已全部完成。**
- ~~**数据绑定铺开**:适配器铺到其余控件(switch/checkbox→bool、bar→value、textinput→str)~~ **已完成:6 个基础控件(第 14 轮)+ Arc/Meter 单向(第 19 轮)**。剩余可选:Dropdown/Tabview/Table 的选中 index 双向绑定(方案已验证可行,按需再做);结构状态(整行/列表绑定)明确不做(与裸机堆约束相悖,LVGL 亦不做);description-driven UI(路径薄壳)看是否需要
- 机制：Tab 键焦点轮转;~~异步双缓冲(buf2 + FlushReady DMA 重叠)~~ **已完成(第 18 轮)**;~~布局(自动排布,免手写坐标)~~ **已完成(第 20 轮:Stack+Align 一次性助手)**。剩余可选:Grid 等分网格、cross-axis stretch(改子 w/h,当前只定位)
- ~~字体：中文(UTF-8 解码 + 稀疏字形 + 按需子集)~~ **已完成(第 17 轮)**;灰度屏抗锯齿已随第 15 轮 AA 落地
- 文档：`CROSS_PLATFORM_PORTING.md` 已覆盖桌面/Android SDL 桥接；仍可按具体 MCU 型号补 SPI/DMA/触摸控制器分步实战
- 低优先修复（第 8 轮记录，暂不动）：Slider 竖直方向假设、TextInput 无横向滚动、1bpp packed 语义统一
- 第 11 轮记录待观察：Dropdown 收起用"隐藏不释放"，弹出层对象在 top_layer 常驻至下次展开/析构；backdrop 展开会全屏标脏(正确但非最小重绘)
- 第 12 轮记录待观察：Table 是自绘型(非子对象),行选中/滚动都在单 draw_cb 内画;大表(数千行)rowAt 是 O(n) 链表遍历,当前只画可见行故实际开销可控,若需海量行可换数组/跳表
