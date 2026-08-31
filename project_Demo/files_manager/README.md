# files_manager — 文件管理器(树形浏览)

project_Demo 第二个基础验证项目。用一个真实小应用验证 YMGUI 是否够用、暴露该补的缺口。
本项目逼出并补齐了库缺口 **TreeView(树形视图控件)**。

## 功能

- **左侧树形浏览**:`YMGUI_TreeView` 展示文件系统层级。文件夹前有三角标记(▶ 收起 / ▼ 展开),
  点三角或双击文件夹切换展开;**展开时懒加载**(`opendir`/`readdir`/`stat`)下一级内容,
  收起只从可见列表摘除、不释放已加载子节点(再展开无需重新读盘)。缩进按层级 16px。
- **右侧文本预览**:选中或双击文本文件(.txt/.md/.c/.h/.cpp/.log/.json/.cfg/.ini),
  内容灌进只读 `YMGUI_TextView`(超 64KB 截断)。非文本文件提示不可预览。
- **顶部工具栏**:
  - `Refresh` 重新扫描沙箱根(整树重载)
  - `New Folder` 在当前目录(选中目录→自身;选中文件→其父;无选中→根)下按名称框内容 `mkdir`
  - `Rename` 把选中项 `rename` 成名称框内容(同目录内)
  - `Delete` 删选中项:文件 `remove`;目录 `rmdir`(**仅空目录**,不递归,安全)
  - 名称输入框(New Folder / Rename 共用)
- **底部**:左侧当前路径,右侧状态栏(操作结果反馈)。

## 沙箱(写操作安全边界)

真实文件系统写操作(mkdir/rename/删除)**全部限定在启动时于系统临时目录建的沙箱内**
(`$TMPDIR/files_manager_sandbox`,默认 `/tmp/files_manager_sandbox`)。启动时自建一棵示例文件树
(readme.txt / notes.md / src/{main.c,util.h,nested/deep.txt} / docs/guide.txt / empty_dir/)。
名称框内容拒绝 `/` 与 `..`,守住沙箱不越界。删除只删单个空目录或单个文件,不做递归 `rm -rf`。

## 构建与运行

```sh
# 在仓库根目录
cmake -S project_Demo/files_manager -B build/rgb565/project_Demo/files_manager
cmake --build build/rgb565/project_Demo/files_manager
# 交互运行
./build/rgb565/project_Demo/files_manager/files_manager
# 无头自检(N 帧后退出;跑一遍 mkdir/rename/rmdir/remove + 树载入自检)
SDL_VIDEODRIVER=dummy ./build/rgb565/project_Demo/files_manager/files_manager 6
```

判成败以 exit code 为准。无头模式打印 `selftest: file ops + tree load OK` 与 `files_manager exit ok`。

## 中文显示

复用 `tools/gb2312_glyphs.bin` 全字库外部 blob(GB2312 码点索引在固件、字模在 blob),
CMake 通过 `GB2312_BIN_PATH` 传绝对路径。无 blob 时中文/标点回退为占位空格(内置 CJK 仅 75 字)。

## 暴露/补齐的库缺口

- **TreeView(本轮新增控件)**:此前 List 每条目建持久子对象、无缩进/无展开标记/无删条目重排,
  做不了树形展开。新做自绘型 `YMGUI_TreeView`(内部节点树 + 拍平的可见行数组,展开/收起重建),
  照 Table/TextView 路子。可裁减宏 `YMGUI_TREEVIEW`(默认 1)。

## 已知限制

- 目录内容排序:目录在前、文件在后,组内按 `readdir` 顺序(未字典序排序)。
- Refresh / 文件操作后整树重载,会丢失已展开状态(收起回未展开)。
- 节点名 64 字节上限;树不横向滚动,长名右侧被裁。
- 预览缓冲 64KB,超大文本文件截断显示。
