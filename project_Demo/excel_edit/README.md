# excel_edit —— 电子表格(第三个基础验证项目)

`project_Demo` 的第三个"真实小应用验证库缺口"的项目。延续约定:txt_edit 催生 `EditView`、
files_manager 催生 `TreeView`,**excel_edit 催生库控件 `Grid`**(通用可编辑单元格网格)。

## 分层

- **库侧 `YMGUI_Grid`**:通用网格控件 —— 单元格寻址、单元格级选中、二维滚动、
  sticky 行列表头(A/B/C 列名 + 1/2/3 行号)、单元格文本存取、编辑意图回调、
  `GetCellRect`(供就地编辑框叠放)。**库不认识"公式"**。
- **app 侧(本项目)**:电子表格语义 —— A1 地址解析、公式引擎、重算、定点格式化。

## 功能

- **公式**:算术 `+ - * / ()`、单元格引用(`=A1+B1*2`)、区间函数
  `SUM/AVG/MIN/MAX(A1:B5)`。
- **数值**:16.16 定点(复用 `GYvalue`/`GY_FP*` 宏,无 FPU),显示 2 位小数。
- **编辑两条路径**:顶部公式栏 `fx:`(编辑当前选中格)+ 单元格内双击就地编辑框。
- **错误**:`#DIV0!`(除零)、`#REF!`(越界/循环引用)、`#ERR!`(语法)。
- **重算**:任一格编辑后全表重算(网格小,够用;循环引用靠 visiting 标志检测)。

## 公式引擎(递归下降)

```
expr    = term (('+'|'-') term)*
term    = factor (('*'|'/') factor)*
factor  = ['-'|'+'] primary
primary = number | addr | func '(' addr ':' addr ')' | '(' expr ')'
```

引用求值经 `evalCell` 递归(带 `visiting` 循环检测,命中 → `#REF!` 防栈溢出)。

## 构建 / 运行

```sh
# 从仓库根:
cmake -S project_Demo/excel_edit -B build/rgb565/project_Demo/excel_edit
cmake --build build/rgb565/project_Demo/excel_edit -j

# 窗口运行:
./build/rgb565/project_Demo/excel_edit/excel_edit

# headless 自检(argv[1]=帧数;判成败以 exit code 为准):
SDL_VIDEODRIVER=dummy ./build/rgb565/project_Demo/excel_edit/excel_edit 6
# 打印 "selftest: formula eval OK" + "excel_edit exit ok",exit 0
```

## 已知取舍

- 全表重算而非依赖图拓扑(网格小,够用)。
- 定容单元格(30 行 × 10 列 A..J,不动态扩)。
- 公式不做字符串函数、不做 `$` 绝对引用、区间仅矩形。
- 显示定点固定 2 位小数;AVG 用整型除计数(近似)。
