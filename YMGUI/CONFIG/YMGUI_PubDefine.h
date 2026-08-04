#ifndef YMGUI_PUBDEFINE_H
#define YMGUI_PUBDEFINE_H

#include <stdio.h>
#include "YMGUI_PubType.h"

#ifndef NULL
#define NULL (void*)0
#endif

//===========================================================================
// 模块裁剪开关(按 Flash/RAM 预算砍,对标 YMCV_XXX_USE)
//===========================================================================
#define YMGUI_WIDGET_BUTTON_USE 1 //按钮控件
#define YMGUI_WIDGET_LABEL_USE  1 //文本标签控件
#define YMGUI_WIDGET_LIST_USE   1 //列表控件
#define YMGUI_FONT_USE          1 //字体子系统

//===========================================================================
// 中文/CJK 字体:三个正交决策,分别在不同时机拍板,别混为一谈
//===========================================================================
// 【轴1 开/关】—— 编译期,本宏裁决
//   YMGUI_FONT_CJK = 1 → 编译 CJK 字模 + 默认字体 fallback 链指向 YMGUI_Font_CJK,
//                        所有控件(全用默认字体 + const char*)零改动即出中文。
//                    0 → 纯 ASCII 足迹:fallback 链断开、FontDataCJK.c 全文 #if 守卫成空、
//                        FontData.c 里 &YMGUI_Font_CJK 引用亦在守卫内(无悬空符号),回最小 flash。
//
// 【轴2 字集范围:方案1 精简 / 方案3 GB2312】—— 字模【生成期】拍板,不是宏!
//   预处理器变不出字模 bitmap:两方案的差别是"哪些汉字被烤进 FontDataCJK.c、文件多大",
//   属于数据不是代码。用宏裁决只是做戏。改法是重跑 Demo/gen_font.py:
//     方案1(默认,精简):python3 Demo/gen_font.py --cjk YMGUI/CORE/YMGUI_FontDataCJK.c
//                        只烤 PRESET_CJK 里列的字(~75 字,9.6KB),控件标题够用,省 flash。
//                        代价:表外的字 fallback 画不出(现象=不显示,非 bug)。
//     方案3(全量):     python3 Demo/gen_font.py --cjk --gb2312 --extern \
//                          --bin Demo/gb2312_glyphs.bin YMGUI/CORE/YMGUI_FontDataGB2312.c
//                        烤 GB2312 一二级共 6763 字(~866KB):索引.c 进库,字模 blob 进外部 flash。
//                        任意简体中文都能显,配轴3外部 flash 回调。
//
// 【轴3 字模存放:内部 rodata / 外部 flash】—— 运行期,靠 GYfont.glyph_read 字段(非本宏)
//   glyph_read == NULL → 直接用 bitmap 指针(内部 flash 或内存映射的 SPI flash)。
//   glyph_read 置回调  → 库先 (font,off,len,buf) 把该字模拷进栈 buf 再 blit
//                        (非映射 SPI flash:整表烧到外部芯片,内部不占;一个 16x16 4bpp 字=128B)。
//   即"用方案3也能放外部 flash":字模数组另行烧录,GYfont 字面量把 bitmap 指向你的
//   flash 地址(映射)或置 glyph_read 回调(非映射),与本头文件无关。
#define YMGUI_FONT_CJK          1

//===========================================================================
// 抗锯齿总开关(编译期,可裁减):
//   1 → 直线/圆弧走 coverage 混合、字体走 4bpp 灰度(默认,边缘平滑)
//   0 → 纯直写硬边(省 flash/cycle;单色 1bpp 屏无中间灰度,内部强制按 0)
//===========================================================================
#ifndef YMGUI_ANTIALIAS
#define YMGUI_ANTIALIAS 1
#endif
//单色屏无法表达中间灰度,强制关闭混合(YMGUI_COLOR_DEPTH 由 PubType.h 定义)
#if YMGUI_COLOR_DEPTH == 1
#undef  YMGUI_ANTIALIAS
#define YMGUI_ANTIALIAS 0
#endif

//脏矩形列表容量上限(超出则退化为合并包围盒;裸机可按 RAM 调小)
#ifndef GY_INV_MAX
#define GY_INV_MAX 16
#endif

//===========================================================================
// 布局助手总开关(编译期,可裁减):
//   1 → 编译 GUI/YMGUI_Layout(Stack/Align 一次性布局工具,无状态零每对象 RAM)
//   0 → 整模块 #if 裁空;库核心不依赖它,手写坐标照常用 → 无悬空符号
//===========================================================================
#ifndef YMGUI_LAYOUT
#define YMGUI_LAYOUT 1
#endif

//===========================================================================
// 树形视图总开关(编译期,可裁减):
//   1 → 编译 WIDGET/YMGUI_TreeView(自绘型层级列表:展开/收起、缩进、懒加载)
//   0 → 整控件 #if 裁空;库核心不依赖它 → 无悬空符号(不用树的项目省这块足迹)
//===========================================================================
#ifndef YMGUI_TREEVIEW
#define YMGUI_TREEVIEW 1
#endif

//===========================================================================
// 网格控件总开关(编译期,可裁减):
//   1 → 编译 WIDGET/YMGUI_Grid(自绘型可编辑单元格网格:单元格选中/编辑意图、
//        二维滚动、sticky 行列表头。电子表格语义在 app 侧,见 project_Demo/excel_edit)
//   0 → 整控件 #if 裁空;库核心不依赖它 → 无悬空符号(不用网格的项目省这块足迹)
//===========================================================================
#ifndef YMGUI_GRID
#define YMGUI_GRID 1
#endif

//===========================================================================
// 画布控件总开关(编译期,可裁减):
//   1 → 编译 WIDGET/YMGUI_Canvas(可写像素缓冲的缩放位图视口:整数缩放/平移、
//        屏→画布像素坐标映射、绘制意图回调。图层栈/融合/合成在 app 侧,见 project_Demo/image_edit)
//   0 → 整控件 #if 裁空;库核心不依赖它 → 无悬空符号(不画图的项目省这块足迹)
//===========================================================================
#ifndef YMGUI_CANVAS
#define YMGUI_CANVAS 1
#endif

//===========================================================================
// 取色器控件总开关(编译期,可裁减):
//   1 → 编译 WIDGET/YMGUI_ColorPicker(HSV 饱和度-明度方块 + 色相条,整数 HSV<->RGB,
//        点击/拖动选色 + changed 回调)
//   0 → 整控件 #if 裁空;库核心不依赖它 → 无悬空符号
//===========================================================================
#ifndef YMGUI_COLORPICKER
#define YMGUI_COLORPICKER 1
#endif

//===========================================================================
// 滚轮/居中高亮列表控件总开关(编译期,可裁减):
//   1 → 编译 WIDGET/YMGUI_Roller(通用等高文本列表:中间行始终居中并高亮,
//        当前位置每帧向目标缓动 → 平滑滚动;可选交互态拖动吸附。歌词/时间/日历/
//        拨码选择器等都可复用,见 project_Demo/music_player 用它做同步歌词)
//   0 → 整控件 #if 裁空;库核心不依赖它 → 无悬空符号
//===========================================================================
#ifndef YMGUI_ROLLER
#define YMGUI_ROLLER 1
#endif

//===========================================================================
// 柱状图控件总开关(编译期,可裁减):
//   1 → 编译 WIDGET/YMGUI_BarChart(通用 N 柱条形显示:每柱按值映射高度。配色模式
//        BY_HEIGHT 随高度渐变 / PER_BAR 每柱独立调色板;顶部回落模式 NONE 无 / BAR 高亮
//        细条峰值保持+回落。频谱/直方图/电平表/统计柱图通用。数据分析在 app 侧,见
//        project_Demo/music_player 用它做实时频谱)
//   0 → 整控件 #if 裁空;库核心不依赖它 → 无悬空符号
//===========================================================================
#ifndef YMGUI_BARCHART
#define YMGUI_BARCHART 1
#endif

//===========================================================================
// 模态对话框控件总开关(编译期,可裁减):
//   1 → 编译 WIDGET/YMGUI_MsgBox(居中卡片 + 全屏遮罩的输入模态对话框:遮罩挂 top_layer
//        吞掉卡片外点击锁死底层,标题/多行正文/N 个按钮各带回调。异步回调式,非线程阻塞。
//        对标 LVGL lv_msgbox。弹窗/告警/确认框通用,见 project_Demo/alarm_clock 到点弹窗)
//   0 → 整控件 #if 裁空;库核心不依赖它 → 无悬空符号(不用弹窗的项目省这块足迹)
//===========================================================================
#ifndef YMGUI_MSGBOX
#define YMGUI_MSGBOX 1
#endif

//===========================================================================
// 常用量宏(对标 YMCV CVMax/CVMin/CVLimitMaxMin)
//===========================================================================
#define GYMax(x, y)              (((x) > (y)) ? (x) : (y))
#define GYMin(x, y)              (((x) < (y)) ? (x) : (y))
#define GYLimitMaxMin(xMin, x, xMax) (((x) > (xMax)) ? (xMax) : ((x) < (xMin)) ? (xMin) : (x))

//位打包单色图:1 bit/像素(对标 YMCV CVGetBin/CVSetBin)
#define GYGetBin(bin8P, index)   ((bin8P)[(index) >> 3] & (1 << ((index) & 7u)))
#define GYSetBin(bin8P, index)   ((bin8P)[(index) >> 3] |= (1 << ((index) & 7u)))
#define GYReSetBin(bin8P, index) ((bin8P)[(index) >> 3] &= ~(1 << ((index) & 7u)))

//===========================================================================
// 颜色转换:GYcolor(ARGB8888) <-> GYpx(按 COLOR_DEPTH)
//   混合路径见 CORE/ 的 blend;此处只做打包/解包
//===========================================================================
//覆盖度常量(GYopa 取值,类型见 PubType.h)
#define GY_OPA_TRANSP 0
#define GY_OPA_COVER  255

#define GY_COLOR_R(c) (((c) >> 16) & 0xFF)
#define GY_COLOR_G(c) (((c) >> 8) & 0xFF)
#define GY_COLOR_B(c) ((c) & 0xFF)
#define GY_COLOR_A(c) (((c) >> 24) & 0xFF)
#define GY_ARGB(a, r, g, b) (((GYcolor)(a) << 24) | ((GYcolor)(r) << 16) | ((GYcolor)(g) << 8) | (GYcolor)(b))

#if YMGUI_COLOR_DEPTH == 16
//ARGB8888 → RGB565: R5 G6 B5
#define GY_ColorToPx(c) ((GYpx)(((GY_COLOR_R(c) & 0xF8) << 8) | ((GY_COLOR_G(c) & 0xFC) << 3) | (GY_COLOR_B(c) >> 3)))
//RGB565 → ARGB8888(补低位,alpha 固定不透明)
#define GY_PxToColor(p) GY_ARGB(0xFF, \
	(((p) >> 11) & 0x1F) << 3, \
	(((p) >> 5) & 0x3F) << 2, \
	((p) & 0x1F) << 3)
#elif YMGUI_COLOR_DEPTH == 8
//ARGB8888 → 灰度: 简化亮度 (R*77 + G*150 + B*29) >> 8
#define GY_ColorToPx(c) ((GYpx)((GY_COLOR_R(c) * 77 + GY_COLOR_G(c) * 150 + GY_COLOR_B(c) * 29) >> 8))
#define GY_PxToColor(p) GY_ARGB(0xFF, (p), (p), (p))
#else //1bpp:非0即白
#define GY_ColorToPx(c) ((GYpx)(((GY_COLOR_R(c) | GY_COLOR_G(c) | GY_COLOR_B(c)) != 0) ? 1 : 0))
#define GY_PxToColor(p) ((p) ? 0xFFFFFFFFu : 0xFF000000u)
#endif

//===========================================================================
// 16.16 定点运算宏(操作 GYvalue/GYcoord,类型见 PubType.h)
//===========================================================================
#define GY_FP_SHIFT 16
#define GY_FP(x)   ((GYvalue)((x) << GY_FP_SHIFT))          //整数→定点
#define GY_INT(fp) ((GYcoord)((fp) >> GY_FP_SHIFT))          //定点→整数(截断)
#define GY_FP_MUL(a, b) ((GYvalue)(((int64)(a) * (b)) >> GY_FP_SHIFT))
#define GY_FP_DIV(a, b) ((GYvalue)(((int64)(a) << GY_FP_SHIFT) / (b)))

//===========================================================================
// 数学/常量封装宏(移植时改宏右边换定点/CMSIS-DSP,对标 YMCV_Sqrt/Sin)
//===========================================================================
#include <math.h>
#define YMGUI_Abs(x)   abs((int)(x))
#define YMGUI_Fabs(x)  fabsf((float)(x))
#define YMGUI_Sin(x)   sinf((float)(x))
#define YMGUI_Cos(x)   cosf((float)(x))
#define YMGUI_Sqrt(x)  sqrtf((float)(x))
#define YMGUI_Atan2(y, x) atan2f((float)(y), (float)(x))
#define YMGUI_Round(x) roundf((float)(x))

#define YMGUI_2Pai   6.2831853f
#define YMGUI_Pai    3.1415926f
#define YMGUI_Deg2Rad 0.0174532922f //(pai/180)
#define YMGUI_Rad2Deg 57.2957805f   //(180/pai)

//文件系统封装(对标 YMCV,字体/资源读取用)
#define YMGUI_FILE   FILE
#define YMGUI_fopen  fopen
#define YMGUI_fread  fread
#define YMGUI_fseek  fseek
#define YMGUI_ftell  ftell
#define YMGUI_fclose fclose

#endif // !YMGUI_PUBDEFINE_H
