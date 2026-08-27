# txt_edit —— 多行文本编辑器

第一个 project_Demo 验证项目。目的**不是**做一个精致编辑器,而是用一个真实小应用跑通"YMGUI 能被独立工程干净复用",并把过程中暴露的库缺口记下来,作为"该补什么"的第一手清单。

- v1 是"行式"编辑器(只读 TextView + 单行 TextInput 逐行追加),暴露"没有可编辑多行控件"这个头号缺口。
- v2 补上 EditView 后升级为全屏多行编辑器(选区/剪贴板/撤销/查找替换 + 状态栏)。
- **v3(当前)= 800×600 窗口 + 顶部菜单栏**(文件/编辑/查找/视图 四个下拉菜单),编辑区一下子大很多。

## 它做什么(v3)

- **窗口 800×600**,顶部一排下拉菜单,下方是占满屏的编辑区,底部状态栏。
- **菜单栏**(用 Dropdown 当菜单:选项 0 是菜单名常显,选后跑动作再复位):
  - 文件 File:New / Clear(清空文档。真实存取需 File I/O,demo 先做清空)
  - 编辑 Edit:Undo / Cut / Copy / Paste / Select All(和键盘快捷键共用同一套 EditView 公开 API)
  - 查找 Find:打开 / 关闭查找替换条
  - 视图 View:Toggle Wrap(切换自动折行)/ Word Count(报字节数 + 显示行数到状态栏)
- 全屏 **EditView** 直接多行编辑:输入/退格/ENTER 换行、方向键(按码点/跨显示行)移光标,中文正确
- **鼠标**:单击定位光标、双击选词、拖动选区
- **选区**:Shift+方向/Home/End 从按下点扩选,Ctrl+A 全选
- **剪贴板**:Ctrl+C 复制 / Ctrl+X 剪切 / Ctrl+V 粘贴(走 HAL 回调缝,SDL 挂系统剪贴板,裸机回退库内静态缓冲)
- **撤销**:Ctrl+Z(单级,连按在两态间切换)
- **行首尾/文首尾**:Home/End、Ctrl+Home/Ctrl+End
- **查找/替换**:Ctrl+F 弹出/收起查找条(查找框实时高亮匹配 + Prev/Next 跳转;替换框 + Repl 单个 / All 全部)
- **状态栏**:底部显示光标 `Ln 行, Col 列`,有选区时追加 `| sel 字节数`

剪贴板后端由 SDL_LCD 在 `SDL_LCD_Init` 里 `YMGUI_Clipboard_SetBackend` 注册;库/控件只调 `YMGUI_Clipboard_Set/GetText`,裸机不注册则自动落到库内静态缓冲。

## 构建 / 运行

```bash
cmake -S project_Demo/txt_edit -B build/project_Demo/txt_edit
cmake --build build/project_Demo/txt_edit -j
./build/project_Demo/txt_edit/txt_edit                          # 有显示时交互跑(800×600)
SDL_VIDEODRIVER=dummy ./build/project_Demo/txt_edit/txt_edit 6  # 无头跑 6 帧(预置多行 + 菜单动作/查找替换/折行自检)
```

## 验证到的:库能用的部分

- 独立工程咬合(vendored `YMGUI/` + 共享 `ymgui_app.cmake`)一次配通,clean build 0 警,headless exit 0。
- EditView 全屏多行编辑 + 选区 + 剪贴板 + 撤销 + 查找/替换 全链路可拼装,状态栏靠 `GetCursorRowCol` / `GetSelection` 每帧刷新。
- **Dropdown 当菜单栏**:选项 0 放菜单名(合起常显)、选后在回调里跑动作再 `SetSelected(0)` 复位,一个控件复用出"菜单"语义。
- 编辑动作(撤销/剪切/复制/粘贴)提成 **EditView 公开 API**(`YMGUI_EditView_Undo/Cut/Copy/Paste`),菜单和键盘快捷键共用同一套,不依赖焦点态。
- Ctrl+F 通过 `EditView_SetFindCb` 把"要查找"的意图转发给上层,查找条 UI(TextInput+Button)完全在 app 侧,控件不管 UI——移植缝干净。
- 剪贴板 HAL 回调缝:同一份库源码,SDL 侧接系统剪贴板,裸机侧零改动落库内缓冲。

## 这次暴露并修掉的库 bug

- **Dropdown 开过又收起后,`CtxFree` 二次释放弹出层(double free)**:菜单收起只隐藏不释放,弹出层常驻 `top_layer`;而 `CtxFree` 原先**先拆 top_layer 子树**(释放了弹出层)**再拆根**(所有者 Dropdown 的 `ddFreeCb→teardownPopup` 又释放一次)。此前 `test_dropdown` 都在 `CtxFree` 前手动 free 掉 dropdown 掩盖了它。修复:`CtxFree` 改为**先释放根子树**(所有者随之 teardown 自己的弹出层并从 top_layer 摘链),再兜底释放 top_layer 剩余。已加回归用例(开→收→不手动 free→CtxFree 不崩)。

## 仍存的缺口

1. **TextInput 无横向滚动**——容量已可在创建时按字段指定，但长文本会画出输入框可视区域。
2. **EditView 文本定容 `GY_EV_TEXT_MAX=2048`**,不动态扩容(裸机优先取舍),超容量插入静默忽略。
3. **撤销仅单级**:连按 Ctrl+Z 在"当前/上一态"间切换,不是多级历史栈。
4. **查找不分大小写、无正则**:纯字节子串匹配。
5. **菜单是 Dropdown 拼的,不是原生菜单栏**:无子菜单、无快捷键提示文本、无 hover 展开;`GY_DROPDOWN_MAX_OPT=16` 限制单菜单项数。够用,但要做成熟菜单还得单立 MenuBar 控件。

## 结论

头号缺口"多行可编辑控件 EditView"补上后,txt_edit 从行式升级为真正的多行编辑器,并把选区/剪贴板/撤销/查找替换这批编辑器常规能力全部落到控件+app 两侧。v3 又用 Dropdown 拼出 文件/编辑/查找/视图 菜单栏(800×600),并借此暴露修掉一个库级 `CtxFree` double-free。复用链路、显示/输入基础件、HAL 移植缝、弹出层生命周期均经真实小应用验证成立。
