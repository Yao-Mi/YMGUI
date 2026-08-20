# YMGUI API 速查手册

所有函数签名对照实际头文件（2026-08，18 控件）。想用某控件时直接抄这里。

## 快速开始：最小程序骨架

```c
#include "YMGUI_PubDefine.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Button.h"
#include "SDL_LCD.h"          // 或你的真实 LCD port
#include "YMGUI_Mem.h"

int main(void)
{
    // 1. 配置显示：draw buffer 可远小于整屏
    GYdisp disp;
    disp.hor_res = 320; disp.ver_res = 240;
    disp.buf_px_cnt = 320 * 40;                       // 一条 band = 40 行
    disp.buf1 = (GYpx*)GY_malloc1(disp.buf_px_cnt * sizeof(GYpx));
    disp.buf2 = NULL; disp.user_data = NULL;

    SDL_LCD_Init(&disp, 2);                           // port 负责挂 flush_cb（真机换这行）

    // 2. 建上下文 + 控件树
    GYCTX ctx = YMGUI_Creat_Ctx_Creat(&disp, 320, 240);
    YMGUI_Obj_SetBgColor(ctx->root, GY_ARGB(0xFF, 0x18, 0x18, 0x20));
    GYOBJ btn = YMGUI_Creat_Button_Creat(ctx->root, 100, 90, 120, 60);
    YMGUI_Button_SetText(btn, "Click Me");

    // 3. 注册接收注入事件的上下文
    YMGUI_Inject_SetCtx(ctx);

    // 4. 主循环：抽事件（可能标脏）→ 刷新（只重绘脏区）
    while (SDL_LCD_PumpEvents()) {
        YMGUI_Refresh(ctx);
        SDL_LCD_Delay(16);
    }

    // 5. 释放（树级联）
    YMGUI_Free_CtxFree(ctx);
    SDL_LCD_Destroy();
    GY_free1(disp.buf1);
    return 0;
}
```

裸机上：去掉 SDL_LCD，主循环里自己轮询触摸/按键调 `YMGUI_Inject_Pointer/Key`，用定时器/vsync 节奏调 `YMGUI_Refresh`。

## 上下文 / 对象 / 生命周期（OPOBJ）

```c
GYCTX YMGUI_Creat_Ctx_Creat(void* disp, GYcoord w, GYcoord h);   // 建上下文+根对象
void  YMGUI_Free_CtxFree(GYCTX ctx);                             // 释放整棵树+ctx
GYOBJ YMGUI_Creat_Obj_Creat(GYOBJ parent, GYcoord x,y,w,h);      // 基础容器/自绘对象
void  YMGUI_Free_ObjFree(GYOBJ obj);                            // 释放对象(递归级联子节点)
void  YMGUI_Obj_GetAbsArea(GYOBJ obj, GYRECT abs);              // 算屏幕绝对矩形
void  YMGUI_Obj_SetBgColor(GYOBJ obj, GYcolor color);          // 设背景色(自动标脏)
```

`ctx->root` 是铺满全屏的根，所有控件挂到它或其子孙下。自定义绘制：`obj->draw_cb = myDraw;`（见 demo_draw.c）。

## 失效 / 刷新（GUI）

```c
void YMGUI_Obj_Invalidate(GYOBJ obj);                    // 标记对象需重绘
void YMGUI_Ctx_InvalidateArea(GYCTX ctx, const GYrect*); // 标记一块屏幕区域
void YMGUI_Refresh(GYCTX ctx);                           // 刷新一帧(无脏区则空转返回)
```

改控件属性的 Set 函数都会自动标脏，通常不用手动 Invalidate。

## 事件 / 焦点 / 输入注入（GUI + HAL）

```c
// 注入（port 调用；裸机把触摸/按键翻译成这些）
void  YMGUI_Inject_SetCtx(void* ctx);                    // 先注册接收事件的 ctx
void  YMGUI_Inject_Pointer(GYcoord x, GYcoord y, uint8 pressed);
void  YMGUI_Inject_PointerCancel(void);                  // 取消捕获,不产生 Clicked
void  YMGUI_Inject_ContextRequest(GYcoord x, GYcoord y); // 一次性右键/上下文操作
void  YMGUI_Inject_ContextBegin(GYcoord x, GYcoord y);   // 捕获式上下文拖动
void  YMGUI_Inject_ContextMove(GYcoord x, GYcoord y);
void  YMGUI_Inject_ContextEnd(GYcoord x, GYcoord y);
void  YMGUI_Inject_ContextCancel(void);
void  YMGUI_Inject_Key(uint32 key, uint8 pressed);
// 内部/进阶
GYOBJ YMGUI_HitTest(GYCTX ctx, GYcoord x, GYcoord y);    // 命中最上层对象
void  YMGUI_SetFocus(GYCTX ctx, GYOBJ obj);              // 设焦点(派发 FocusLost/Got)
```

键码：可打印字符用 ASCII；控制键用 `GY_KEY_BACKSPACE/ENTER/LEFT/RIGHT/UP/DOWN/DEL`。
事件类型（控件 event_cb 收）：`GY_EVENT_Pressed/Pressing/Released/ReleasedOff/Clicked/DoubleClicked/ContextRequested/ContextDragging/ContextReleased/ContextCancelled/FocusGot/FocusLost/Key`。
鼠标右键和触摸长按统一为上下文语义，不等价于普通 `Clicked`，也不自动改变焦点。右键短点击只派一次 `ContextRequested`；右键拖动和长按后拖动走 `ContextRequested → ContextDragging* → ContextReleased/ContextCancelled`，捕获对象保存在 `ctx->context_obj`。`PointerCancel` 会清除普通按下状态并派 `ReleasedOff`，但不会派 `Clicked`。
坐标/键值从 `ctx->point_x`、`ctx->point_y`、`ctx->last_key` 读。SDL 触摸长按依靠周期调用 `SDL_LCD_PumpEvents()` 检查超时。

## 控件目录

每个控件都是 `GYOBJ`，都用 `YMGUI_Creat_XXX_Creat(parent, x, y, w, h)` 创建、`YMGUI_Free_ObjFree` 释放。下面只列各自的专有函数。

### Label 文本标签
```c
GYOBJ YMGUI_Creat_Label_Creat(parent, x,y,w,h);
void  YMGUI_Label_SetText(GYOBJ label, const char* text);   // 文字(居中)
void  YMGUI_Label_SetTextColor(GYOBJ label, GYcolor color);
void  YMGUI_Label_SetBgEnable(GYOBJ label, uint8 enable);   // 是否画背景
```

### Button 按钮
```c
GYOBJ YMGUI_Creat_Button_Creat(parent, x,y,w,h);
void  YMGUI_Button_SetText(GYOBJ btn, const char* text);    // 居中标题
void  YMGUI_Button_SetColors(GYOBJ btn, GYcolor normal, GYcolor pressed);
void  YMGUI_Button_SetClicked(GYOBJ btn, void(*cb)(GYOBJ)); // 点击回调
void  YMGUI_Button_SetImage(GYOBJ btn, GYIMG src);          // 居中贴图(不拥有像素;NULL=清图回退文字)。有图则不画文字
void  YMGUI_Button_SetBgVisible(GYOBJ btn, uint8 on);       // 底色+边框是否画(默认1;关掉=纯图标/透明按钮)
```
> 图优先:设了图源就居中 blit 图、不画文字。状态切换(如播放↔暂停)由 app 调 `SetImage` 换图(同 `SetText` 换字);异形图标靠 `GYimg.use_key + key` colorkey 抠形。

### Checkbox 复选框
```c
GYOBJ YMGUI_Creat_Checkbox_Creat(parent, x,y,w,h);
void  YMGUI_Checkbox_SetText(GYOBJ cb, const char* text);
void  YMGUI_Checkbox_SetChecked(GYOBJ cb, uint8 checked);
uint8 YMGUI_Checkbox_GetChecked(GYOBJ cb);
void  YMGUI_Checkbox_SetChanged(GYOBJ cb, void(*cb_fn)(GYOBJ, uint8 checked));
```

### Switch 开关
```c
GYOBJ YMGUI_Creat_Switch_Creat(parent, x,y,w,h);
void  YMGUI_Switch_SetOn(GYOBJ sw, uint8 on);
uint8 YMGUI_Switch_GetOn(GYOBJ sw);
void  YMGUI_Switch_SetChanged(GYOBJ sw, void(*cb)(GYOBJ, uint8 on));
```

### Slider 滑块（可拖动）
```c
GYOBJ YMGUI_Creat_Slider_Creat(parent, x,y,w,h);           // 假设 w>h(水平)
void  YMGUI_Slider_SetRange(GYOBJ sld, int32 min, int32 max);
void  YMGUI_Slider_SetValue(GYOBJ sld, int32 value);       // 钳制
int32 YMGUI_Slider_GetValue(GYOBJ sld);
void  YMGUI_Slider_SetChanged(GYOBJ sld, void(*cb)(GYOBJ, int32 value));
```

### Bar 进度条（只显示）
```c
GYOBJ YMGUI_Creat_Bar_Creat(parent, x,y,w,h);
void  YMGUI_Bar_SetRange(GYOBJ bar, int32 min, int32 max);
void  YMGUI_Bar_SetValue(GYOBJ bar, int32 value);
int32 YMGUI_Bar_GetValue(GYOBJ bar);
void  YMGUI_Bar_SetColors(GYOBJ bar, GYcolor bg, GYcolor fg);
```

### Image 图片
```c
GYOBJ YMGUI_Creat_Image_Creat(parent, x,y,w,h);
void  YMGUI_Image_SetSrc(GYOBJ img, GYIMG src);            // 居中 blit,不复制像素
// GYimg 描述符: { const GYpx* data; GYcoord w,h; uint8 use_key; GYpx key; }
```

### Arc 环形进度
```c
GYOBJ YMGUI_Creat_Arc_Creat(parent, x,y,w,h);
void  YMGUI_Arc_SetRange(GYOBJ arc, int32 min, int32 max);
void  YMGUI_Arc_SetValue(GYOBJ arc, int32 value);
int32 YMGUI_Arc_GetValue(GYOBJ arc);
void  YMGUI_Arc_SetAngles(GYOBJ arc, int32 start_deg, int32 end_deg); // 0=右 90=下
void  YMGUI_Arc_SetWidth(GYOBJ arc, GYcoord thickness);
void  YMGUI_Arc_SetColors(GYOBJ arc, GYcolor bg, GYcolor fg);
```

### Spinner 加载转圈
```c
GYOBJ YMGUI_Creat_Spinner_Creat(parent, x,y,w,h);
void  YMGUI_Spinner_Tick(GYOBJ sp);                        // 主循环每帧调,推进旋转
void  YMGUI_Spinner_SetSpan(GYOBJ sp, int32 span_deg, int32 step_deg);
void  YMGUI_Spinner_SetColor(GYOBJ sp, GYcolor color);
void  YMGUI_Spinner_SetWidth(GYOBJ sp, GYcoord thickness);
```

### Meter 仪表盘
```c
GYOBJ YMGUI_Creat_Meter_Creat(parent, x,y,w,h);
void  YMGUI_Meter_SetRange(GYOBJ m, int32 min, int32 max);
void  YMGUI_Meter_SetValue(GYOBJ m, int32 value);
int32 YMGUI_Meter_GetValue(GYOBJ m);
void  YMGUI_Meter_SetAngles(GYOBJ m, int32 start_deg, int32 end_deg);
void  YMGUI_Meter_SetTicks(GYOBJ m, uint8 count);          // 刻度数
```

### TextInput 文本输入框（可聚焦编辑）
```c
GYOBJ YMGUI_Creat_TextInput_Creat(parent, x,y,w,h);        // 自带 Focusable
void  YMGUI_TextInput_SetText(GYOBJ ti, const char* text); // 光标移末尾
const char* YMGUI_TextInput_GetText(GYOBJ ti);
void  YMGUI_TextInput_SetChanged(GYOBJ ti, GYti_changed_cb cb);// 内容变才触发
// 点击聚焦后,Inject_Key 自动处理插入/退格/Del/左右移光标
// 中文(UTF-8)可输入:SDL 逐字节注入,按整码点插入/退格/移光标,光标像素定位支持中英混排
```

### EditView 可编辑多行文本框（光标 + 换行 + 中文）
```c
GYOBJ YMGUI_Creat_EditView_Creat(parent, x,y,w,h, size_t capacity);// capacity=可用字节,按需申报(密码框几十 B / 编辑器 128K)
void  YMGUI_EditView_SetText(GYOBJ ev, const char* text);   // 拷进内部缓冲(超 capacity 截断);NULL/"" 清空
const char* YMGUI_EditView_GetText(GYOBJ ev);
void  YMGUI_EditView_SetWrap(GYOBJ ev, uint8 on);           // 按宽度自动折行(默认关)
void  YMGUI_EditView_SetTextColor(GYOBJ ev, GYcolor color);
void  YMGUI_EditView_SetBgColor(GYOBJ ev, GYcolor color);    // 编辑区底色
void  YMGUI_EditView_SetBorderColor(GYOBJ ev, GYcolor color);// 1px 外框;alpha=0 则不画
void  YMGUI_EditView_SetUndoEnabled(GYOBJ ev, uint8 on);     // 撤销默认开(占 2x capacity);关掉省一半 RAM
void  YMGUI_EditView_SetScroll(GYOBJ ev, int32 scroll_y);   // 钳到内容范围(大文件像素高超 int16,用 int32)
int32 YMGUI_EditView_GetScroll(GYOBJ ev);
size_t  YMGUI_EditView_GetLineCount(GYOBJ ev);             // 断行/折行后的显示行数
size_t  YMGUI_EditView_GetCursor(GYOBJ ev);                // 光标字节位置
void  YMGUI_EditView_SetChanged(GYOBJ ev, GYev_changed_cb cb);
void  YMGUI_EditView_SetFindCb(GYOBJ ev, GYev_action_cb cb); // Ctrl+F 转发"要查找"意图给上层
// --- 选区 ---
uint8  YMGUI_EditView_HasSelection(GYOBJ ev);
void   YMGUI_EditView_GetSelection(GYOBJ ev, size_t* start, size_t* end); // [start,end) 字节,可 NULL
void   YMGUI_EditView_SelectAll(GYOBJ ev);
void   YMGUI_EditView_ClearSelection(GYOBJ ev);
size_t YMGUI_EditView_GetSelectionText(GYOBJ ev, char* out, size_t out_cap);
// --- 编辑动作(菜单/快捷键共用,不依赖焦点态)---
void   YMGUI_EditView_Undo/Copy/Cut/Paste(GYOBJ ev);
// --- 光标行列(1 基,供状态栏)---
void   YMGUI_EditView_GetCursorRowCol(GYOBJ ev, size_t* row, size_t* col);
// --- 查找/替换(纯字节子串,区分大小写)---
void   YMGUI_EditView_SetFindNeedle(GYOBJ ev, const char* needle);       // 匹配高亮,不移光标
uint8  YMGUI_EditView_FindNext/FindPrev(GYOBJ ev, const char* needle);   // 命中选中+滚入可见
uint8  YMGUI_EditView_Replace(GYOBJ ev, const char* needle, const char* repl);
size_t YMGUI_EditView_ReplaceAll(GYOBJ ev, const char* needle, const char* repl); // 返回替换次数
// 聚焦后编辑:UTF-8 字节插入/ENTER 换行/退格跨行合并/Del/方向键(LEFT/RIGHT 按码点,UP/DOWN 跨行);
// 编辑后光标行自动滚入可见区;拖动即纵向滚动。中文从头做对(整码点,像素光标定位)
// 容量:创建期按需申报(text 缓冲堆分配,undo 默认再占一份);长度/光标/行数用 size_t;剪贴板 GY_CLIP_MAX=32KB
// GY_EV_TEXT_MAX=131072 仅为"要大编辑器"的调用方提供的建议值,不是控件内部固定尺寸
```

### TextView 多行文本视图（只读，可滚动）
```c
GYOBJ YMGUI_Creat_TextView_Creat(parent, x,y,w,h);          // 空文本,Multiline 开/Wrap 关
void  YMGUI_TextView_SetText(GYOBJ tv, const char* text);   // 深拷一份;NULL/"" 清空
const char* YMGUI_TextView_GetText(GYOBJ tv);
void  YMGUI_TextView_SetMultiline(GYOBJ tv, uint8 on);      // 遇 '\n' 断行(默认开)
void  YMGUI_TextView_SetWrap(GYOBJ tv, uint8 on);           // 按宽度自动折行(默认关=长行右裁)
void  YMGUI_TextView_SetLineHeight(GYOBJ tv, GYcoord line_h);
void  YMGUI_TextView_SetTextColor(GYOBJ tv, GYcolor color);
void  YMGUI_TextView_SetScroll(GYOBJ tv, GYcoord scroll_y); // 钳到内容范围
GYcoord YMGUI_TextView_GetScroll(GYOBJ tv);
uint16  YMGUI_TextView_GetLineCount(GYOBJ tv);              // 断行/折行后的显示行数
// 拖动即纵向滚动;只读,无编辑光标(编辑用 TextInput)
```

### List 可滚动列表
```c
GYOBJ YMGUI_Creat_List_Creat(parent, x,y,w,h);             // 视口,子项超出被裁
GYOBJ YMGUI_List_AddItem(GYOBJ list, const char* text, GYcoord item_h); // 返回条目对象
void  YMGUI_List_SetScroll(GYOBJ list, GYcoord scroll_y);  // 钳到内容范围
GYcoord YMGUI_List_GetScroll(GYOBJ list);
// 拖动列表(条目或空白)即滚动,无需手动处理
```

### TreeView 树形视图（自绘型层级列表：展开/收起、缩进、懒加载）
```c
GYOBJ YMGUI_Creat_TreeView_Creat(parent, x,y,w,h);                 // 空树
// --- 建树(节点是轻量结构,非 GYOBJ;深拷名字≤63 字节)---
GYTREENODE YMGUI_TreeView_AddNode(GYOBJ tree, GYTREENODE parent_node, const char* name, uint8 is_dir); // parent=NULL→根
void  YMGUI_TreeView_ClearChildren(GYOBJ tree, GYTREENODE node);   // 清子树(保留 node);清选区防悬空
void  YMGUI_TreeView_Clear(GYOBJ tree);                            // 清空整棵树
// --- 展开态 ---
void  YMGUI_TreeView_SetExpanded(GYOBJ tree, GYTREENODE node, uint8 expanded); // 首次展开触发 expand_cb
uint8 YMGUI_TreeView_IsExpanded(GYTREENODE node);
// --- 节点读写 ---
const char* YMGUI_TreeView_NodeName(GYTREENODE node);
uint8 YMGUI_TreeView_NodeIsDir(GYTREENODE node);
GYTREENODE YMGUI_TreeView_NodeParent(GYTREENODE node);
void* YMGUI_TreeView_NodeUserPtr(GYTREENODE node);                 // 挂用户指针(如路径/元数据)
void  YMGUI_TreeView_SetNodeUserPtr(GYTREENODE node, void* p);
// --- 选中 ---
GYTREENODE YMGUI_TreeView_GetSelectedNode(GYOBJ tree);
void  YMGUI_TreeView_SetSelectedNode(GYOBJ tree, GYTREENODE node);
// --- 回调 ---
void  YMGUI_TreeView_SetExpandCb(GYOBJ tree, GYtree_expand_cb cb);   // 懒加载:首次展开目录时填子节点
void  YMGUI_TreeView_SetSelectCb(GYOBJ tree, GYtree_select_cb cb);   // 单击选中
void  YMGUI_TreeView_SetActivateCb(GYOBJ tree, GYtree_activate_cb cb);// 双击文件(目录双击=切展开)
// --- 外观/滚动 ---
void  YMGUI_TreeView_SetRowHeight(GYOBJ tree, GYcoord row_h);
void  YMGUI_TreeView_SetIndent(GYOBJ tree, GYcoord indent);         // 每层缩进像素(默认 16)
void  YMGUI_TreeView_SetScroll(GYOBJ tree, GYcoord scroll_y);       // 钳到内容范围
GYcoord YMGUI_TreeView_GetScroll(GYOBJ tree);
uint16  YMGUI_TreeView_GetVisibleCount(GYOBJ tree);                 // 当前展开态下的可见行数
// 节点树(child_head/sibling 链)+ 展平可见数组(展开/收起时重建);只画可见行。
// 单击标记(▶/▼)切展开;单击名字选中;双击目录切展开、双击文件触发 activate。拖动即滚动。
// 目录名暖黄、文件浅灰;懒加载 loaded 标志防重入。编译期总开关 YMGUI_TREEVIEW(0=整控件裁空)
```

### Canvas 可绘制位图视口（自持 GYpx 显示缓冲；缩放/平移/绘制回调）

```c
typedef enum { GY_CANVAS_DOWN=0, GY_CANVAS_MOVE, GY_CANVAS_UP } GYcanvas_phase;
typedef void (*GYcanvas_paint_cb)(GYOBJ canvas, GYcanvas_phase phase, int32 cx, int32 cy, uint8 in_bounds);

GYOBJ  YMGUI_Creat_Canvas_Creat(parent, x,y,w,h, uint16 cw, uint16 ch);  // 画布像素 cw*ch,初白
uint16 YMGUI_Canvas_GetW(GYOBJ canvas);
uint16 YMGUI_Canvas_GetH(GYOBJ canvas);
GYpx*  YMGUI_Canvas_GetBuffer(GYOBJ canvas);       // cw*ch 行优先显示缓冲,app 合成后直接写这里
void   YMGUI_Canvas_Invalidate(GYOBJ canvas);      // 写缓冲后需调此才上屏
void   YMGUI_Canvas_SetZoom(GYOBJ canvas, uint8 zoom);   // 整数倍 1..GY_CANVAS_ZOOM_MAX(16)
uint8  YMGUI_Canvas_GetZoom(GYOBJ canvas);
void   YMGUI_Canvas_SetPan/GetPan(GYOBJ canvas, int32 pan_x, int32 pan_y);   // 平移(留≥16px 可见,小于视口则居中)
void   YMGUI_Canvas_SetPanMode/GetPanMode(GYOBJ canvas, uint8 on);           // 手型:拖动改 pan 而非绘制
uint8  YMGUI_Canvas_ScreenToCanvas(GYOBJ canvas, GYcoord sx,sy, int32* cx,int32* cy);// 屏→画布(负坐标 floor 除),返回 in_bounds
uint8  YMGUI_Canvas_IsDrawing(GYOBJ canvas, int32* cx,int32* cy);            // 按住绘制中?供喷枪逐帧驱动
void   YMGUI_Canvas_SetPaintCb(GYOBJ canvas, GYcanvas_paint_cb cb);          // DOWN/MOVE/UP + 画布坐标 + in_bounds
// 通用可变位图视口(库无图层/融合概念——那些在 app 侧)。type 保持 GY_OBJ_Base 自绘。
// 编译期总开关 YMGUI_CANVAS(0=整控件裁空)
```

### ColorPicker HSV 取色器（SV 方块 + 色相条，整数 HSV↔RGB 无 FPU）

```c
typedef void (*GYcolorpicker_cb)(GYOBJ picker, GYcolor color);

GYOBJ   YMGUI_Creat_ColorPicker_Creat(parent, x,y,w,h);
void    YMGUI_ColorPicker_SetColor(GYOBJ picker, GYcolor color);   // 不触发回调
GYcolor YMGUI_ColorPicker_GetColor(GYOBJ picker);
void    YMGUI_ColorPicker_SetHSV(GYOBJ picker, uint16 h, uint8 s, uint8 v);  // 不触发回调
void    YMGUI_ColorPicker_GetHSV(GYOBJ picker, uint16* h, uint8* s, uint8* v);
void    YMGUI_ColorPicker_SetChangedCb(GYOBJ picker, GYcolorpicker_cb cb);   // 拖动即回调
GYcolor YMGUI_ColorPicker_HSVtoRGB(uint16 h, uint8 s, uint8 v);    // h 0..359, s/v 0..255(整数,可独立调用)
void    YMGUI_ColorPicker_RGBtoHSV(GYcolor color, uint16* h, uint8* s, uint8* v);
// 右侧色相条选 H,左侧 SV 方块选 S(x)/V(y)。库不认识"当前墨色/工具"——那在 app 侧。
// 编译期总开关 YMGUI_COLORPICKER(0=整控件裁空)
```

### Roller 居中高亮平滑滚动列表（歌词/时间/日历通用）

```c
typedef void (*GYroller_changed_cb)(GYOBJ roller, int32 index);

GYOBJ YMGUI_Creat_Roller_Creat(parent, x,y,w,h);
void  YMGUI_Roller_SetLines(GYOBJ roller, const char* const* lines, int32 n);  // 整批设行(深拷)
int32 YMGUI_Roller_AddLine(GYOBJ roller, const char* text);                    // 追加一行,返回新行数
void  YMGUI_Roller_Clear(GYOBJ roller);
int32 YMGUI_Roller_GetCount(GYOBJ roller);
void  YMGUI_Roller_SetSelected(GYOBJ roller, int32 index, uint8 animate);      // animate=1 缓动,=0 立即
int32 YMGUI_Roller_GetSelected(GYOBJ roller);
void  YMGUI_Roller_SetVisibleRows(GYOBJ roller, int32 rows);                   // 可见行数(奇数为宜)
void  YMGUI_Roller_SetRowHeight(GYOBJ roller, GYcoord row_h);
void  YMGUI_Roller_SetColors(GYOBJ roller, GYcolor bg, GYcolor normal, GYcolor hi, GYcolor hi_bg); // hi_bg=0 不画底
void  YMGUI_Roller_SetInteractive(GYOBJ roller, uint8 on);                     // 开=拖动滚动+吸附;默认关(纯程序驱动)
void  YMGUI_Roller_SetEaseDiv(GYOBJ roller, int32 div);                        // 缓动速度(越大越慢,默认4;<=0 立即到位)
void  YMGUI_Roller_SetChanged(GYOBJ roller, GYroller_changed_cb cb);
uint8 YMGUI_Roller_Tick(GYOBJ roller);                                         // 每帧推进缓动,返回是否仍在动
// 选中行永远绘制在控件竖直正中并高亮,其余行上下排开越界裁掉;16.16 定点位置每帧向目标行缓动。
// 两用:程序态(只 SetSelected,歌词/时间/日历用)/ 交互态(拖动改滚动,抬起吸附最近行触发 changed)。
// 库不认识"歌词"——行文本由 app 喂(如 .lrc 解析后按时钟 SetSelected)。
// 编译期总开关 YMGUI_ROLLER(0=整控件裁空)
```

### BarChart 通用柱状图（频谱/直方图/电平表/统计柱图通用）

```c
GYOBJ YMGUI_Creat_BarChart_Creat(parent, x,y,w,h);
void  YMGUI_BarChart_SetBarCount(GYOBJ bc, int32 n);                // 钳到 [1, GY_BARCHART_MAX_BARS(64)]
int32 YMGUI_BarChart_GetBarCount(GYOBJ bc);
void  YMGUI_BarChart_SetRange(GYOBJ bc, int32 max);                 // value 映射到 [0,max]→高度
void  YMGUI_BarChart_SetValue(GYOBJ bc, int32 i, int32 value);      // 设某柱值;TopMode=BAR 时更高则同顶起顶标
void  YMGUI_BarChart_SetValues(GYOBJ bc, const int32* arr, int32 n);
int32 YMGUI_BarChart_GetValue(GYOBJ bc, int32 i);
// —— 配色模式(两选一)——
void  YMGUI_BarChart_SetColorMode(GYOBJ bc, uint8 mode);            // GY_BARCHART_COLOR_BY_HEIGHT(默认) / _PER_BAR
int32 YMGUI_BarChart_GetColorMode(GYOBJ bc);
void  YMGUI_BarChart_SetGradient(GYOBJ bc, GYcolor lo, GYcolor hi); // BY_HEIGHT:柱身逐行竖直渐变(底 lo→顶 hi)
void  YMGUI_BarChart_SetBarColor(GYOBJ bc, int32 i, GYcolor color); // PER_BAR:设某柱独立色
void  YMGUI_BarChart_SetBarColors(GYOBJ bc, const GYcolor* arr, int32 n); // PER_BAR:整批调色板
void  YMGUI_BarChart_SetBgColor(GYOBJ bc, GYcolor bg);
// —— 顶部回落(峰值)——
void  YMGUI_BarChart_SetTopMode(GYOBJ bc, uint8 mode);              // GY_BARCHART_TOP_BAR(默认,高亮细条) / _NONE(无)
void  YMGUI_BarChart_SetTopColor(GYOBJ bc, GYcolor color);          // 顶标条色
void  YMGUI_BarChart_SetDecay(GYOBJ bc, int32 fall, int32 top_fall);// 柱身/顶标每帧回落量(<0 保持)
void  YMGUI_BarChart_SetGap(GYOBJ bc, GYcoord gap);                 // 柱间空隙(默认2)
uint8 YMGUI_BarChart_Tick(GYOBJ bc);                                // 每帧衰减回落,返回是否仍在动
// 只显示不交互。两个正交维度组合出多种柱图:
//   配色 BY_HEIGHT(柱身底 lo→顶 hi 逐行竖直渐变,同一物理量不同强度,频谱/电平表)/ PER_BAR(每柱一色,不同类目)
//   顶标 BAR(峰值悬停+回落,频谱手感)/ NONE(纯柱)。SetDecay(0,0)+TOP_NONE = 静态普通柱状图。
//   顶标只做高亮条那种;要文字顶标(数值随柱头)由 app 叠一个 Label,库不引 Font 依赖。
// 库不做数据分析——值由 app 侧喂进来(频谱见 music_player 的 mp_spectrum 整数 Goertzel)。
// 编译期总开关 YMGUI_BARCHART(0=整控件裁空)
```

## 底层图元（CORE，通常控件内部用；自绘 draw_cb 里可直接调）

收 `GYSURFACE s`（draw_cb 收不到 surface，需在自绘对象的 draw_cb 里用传入的 s）。坐标为屏幕绝对坐标，自动裁剪。

```c
void YMGUI_Draw_Fill(GYSURFACE s, const GYrect* area, GYcolor color, GYopa opa);
void YMGUI_Draw_Line(GYSURFACE s, GYcoord x1,y1,x2,y2, GYcolor color);          // Bresenham
void YMGUI_Draw_Img (GYSURFACE s, GYIMG img, GYcoord x, GYcoord y);             // blit
void YMGUI_Draw_Circle    (GYSURFACE s, GYcoord cx,cy,r, GYcolor color);        // 圆环
void YMGUI_Draw_CircleFill(GYSURFACE s, GYcoord cx,cy,r, GYcolor color);        // 实心盘
void YMGUI_Draw_Arc  (GYSURFACE s, GYcoord cx,cy,r, int32 s_deg,e_deg, GYcolor);// 弧(1px)
void YMGUI_Draw_ArcThick(GYSURFACE s, GYcoord cx,cy,r_in,r_out, int32 s,e, GYcolor);// 粗弧环(无缝)
// 文字
GYcoord YMGUI_Draw_Glyph(GYSURFACE s, GYFONT font, GYcoord x,y, uint32 cp, GYcolor);// 按码点画一字
GYcoord YMGUI_Draw_Char(GYSURFACE s, GYFONT font, GYcoord x,y, char c, GYcolor);
GYcoord YMGUI_Draw_Text(GYSURFACE s, GYFONT font, GYcoord x,y, const char* str, GYcolor);// UTF-8
GYcoord YMGUI_Draw_TextN(GYSURFACE s, GYFONT font, GYcoord x,y, const char* str, uint32 nbytes, GYcolor);// 画前 nbytes 字节(非结尾子串,如 TextView 一行)
GYcoord YMGUI_Font_TextWidth(GYFONT font, const char* str);                          // UTF-8
GYcoord YMGUI_Font_TextWidthN(GYFONT font, const char* str, uint32 nbytes);          // 量前 nbytes 字节
// 默认字体: extern const GYfont YMGUI_Font_Default;  (8x16 ASCII, fallback→CJK)
```

## 中文 / CJK 字体

Draw_Text / TextWidth 按 UTF-8 解码码点;默认 ASCII 字体遇 CJK 码点自动沿 `fallback`
链回退到 `YMGUI_Font_CJK`(16x16 4bpp)。**所有控件用默认字体 + `const char*`,标签/按钮
直接塞 UTF-8 中文即可,无一处控件改动。** 三个正交决策(完整说明见 `CONFIG/YMGUI_PubDefine.h`):

| 决策 | 时机 | 机制 | 选项 |
|---|---|---|---|
| CJK 开/关 | 编译期 | 宏 `YMGUI_FONT_CJK` | `1`=带中文 / `0`=纯 ASCII 足迹 |
| 字集范围 | 字模**生成期** | `gen_font.py` 参数(**非宏**) | 方案1 精简(`--cjk`,~75字/9.6KB) / 方案3 GB2312(`--cjk --gb2312`,6763字/~866KB) |
| 字模存放 | 运行期 | `GYfont.glyph_read` 字段 | `NULL`=直接指针(内部/映射flash) / 置回调=拷贝再blit(非映射SPI flash) |

字集范围**不用宏裁决**:两方案差别是"哪些字被烤进 `FontDataCJK.c`",属数据非代码,
预处理器变不出字模。换字集=重跑 `Demo/gen_font.py`(用法见其脚本头)。方案3 太大时配
"字模存放=外部 flash":整表烧到 SPI 芯片,GYfont 字面量置 `glyph_read` 回调,内部 flash 不占。
```c
// 外部非映射 flash 取字模回调签名(一个 16x16 4bpp 字 = 128 字节;第 i 字在 i*128 偏移):
uint32 my_read(const GYfont* f, uint32 off, uint32 len, uint8* buf);
```

**运行期全局兜底钩子**(让方案3/外部 flash 字体自动灌进所有控件,零控件改动):
```c
void  YMGUI_Font_SetFallback(GYFONT font);  // 挂全局兜底字体(默认 NULL);任一字体 const
GYFONT YMGUI_Font_GetFallback(void);         //   fallback 链走空后再下探它。传 NULL 解除
```
app 自造 GYfont(`bitmap=NULL`,填 `glyph_read` 回调)引用固件内的码点索引表(`--extern`
生成的 `YMGUI_GB2312_cps[]`),`YMGUI_Font_SetFallback(&it)` 一句即让所有控件遇任意 GB2312
汉字自动走外部 flash。代价仅 `.bss` 一个指针,字体数据本身全 const。见 `demo_font_gb2312.c`。

## 颜色 / 常用宏（CONFIG）

```c
GY_ARGB(a, r, g, b)          // 造 GYcolor,如 GY_ARGB(0xFF,0x30,0x90,0xE0)
GY_OPA_COVER  (255) / GY_OPA_TRANSP (0)
GYMax(a,b) / GYMin(a,b) / GYLimitMaxMin(min,x,max)
```

## 定点三角（COMMON/MATH，画弧/旋转用）

```c
int16 GY_Sin(int32 deg);   int16 GY_Cos(int32 deg);      // Q15(值=实际*32768)
// 极坐标点: px = cx + (r * GY_Cos(deg) >> 15);  py = cy + (r * GY_Sin(deg) >> 15);
```

## 速查：想干某事看哪个 demo

| 想学 | 看 |
|------|-----|
| 最小 band 刷新闭环 | Demo/demo_flush_band.c |
| 按钮 + 点击回调 + 计数标签 | Demo/demo_button.c |
| 表单四件套联动 | Demo/demo_form.c |
| 仪表盘/环形/转圈动画 | Demo/demo_dashboard.c |
| 自绘 draw_cb 里用图元 | Demo/demo_draw.c |
| 文本输入 + 焦点 | Demo/demo_textinput.c |
| 可滚动列表 | Demo/demo_list.c |
| 可绘制位图画布(缩放/绘制回调) | Demo/demo_canvas.c |
| HSV 取色器 | Demo/demo_colorpicker.c |
| 居中高亮平滑滚动列表(歌词/时间/日历) | Demo/demo_roller.c |
| 通用柱状图(频谱/直方图/统计) | Demo/demo_barchart.c |
| 类 PS 图层画板(图层/融合/工具全 app 侧) | project_Demo/image_edit/ |
| 音乐播放器(ffmpeg 解码/SDL2 声卡/歌词/频谱全 app 侧) | project_Demo/music_player/ |
| 任意机制的精确行为 | 对应 Demo/test_*.c（断言即规格） |
