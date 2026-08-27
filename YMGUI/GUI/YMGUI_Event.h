#ifndef YMGUI_EVENT_H
#define YMGUI_EVENT_H

#include "YMGUI_PubType.h"
#include "YMGUI_Obj.h"

//===========================================================================
// 事件分发 + 命中测试
//   HAL 注入的指针/按键 → 找到命中对象 → 派发事件 → 控件改状态并标脏
//===========================================================================

//特殊键值(可打印字符直接用其 ASCII;控制键用高位段避免冲突)
#define GY_KEY_BACKSPACE 0x08
#define GY_KEY_ENTER     0x0D
#define GY_KEY_LEFT      0x1000
#define GY_KEY_RIGHT     0x1001
#define GY_KEY_UP        0x1002
#define GY_KEY_DOWN      0x1003
#define GY_KEY_DEL       0x1004
#define GY_KEY_TAB       0x1005  //先派发给焦点控件;控件未消费(未置 key_handled)才轮转焦点
#define GY_KEY_HOME      0x1006  //行首
#define GY_KEY_END       0x1007  //行尾

//编辑器扩展键(0x1100 段):修饰键组合由 HAL 层(SDL_LCD)检测后合成为这些虚拟键,
//库/控件只认虚拟键,不需要 ctx 存修饰键位。见 SDL_LCD.c 的 SDL_GetModState 分支。
#define GY_KEY_SHIFT_LEFT   0x1100  //Shift+方向:从锚点扩选到光标
#define GY_KEY_SHIFT_RIGHT  0x1101
#define GY_KEY_SHIFT_UP     0x1102
#define GY_KEY_SHIFT_DOWN   0x1103
#define GY_KEY_SHIFT_HOME   0x1104  //Shift+Home/End:扩选到行首/行尾
#define GY_KEY_SHIFT_END    0x1105
#define GY_KEY_DOC_HOME     0x1106  //Ctrl+Home/End:光标到文首/文尾
#define GY_KEY_DOC_END      0x1107
#define GY_KEY_SEL_ALL      0x1108  //Ctrl+A 全选
#define GY_KEY_COPY         0x1109  //Ctrl+C 复制选区
#define GY_KEY_CUT          0x110A  //Ctrl+X 剪切选区
#define GY_KEY_PASTE        0x110B  //Ctrl+V 粘贴
#define GY_KEY_UNDO         0x110C  //Ctrl+Z 撤销(单级)
#define GY_KEY_FIND         0x110D  //Ctrl+F 查找(上层 app 处理,弹查找条)

//命中测试:返回 (x,y) 处最上层(最后添加/最深)的对象,无则返回 root
GYOBJ YMGUI_HitTest(GYCTX ctx, GYcoord x, GYcoord y);

//处理一次指针状态(由 HAL 的 YMGUI_Inject_Pointer 转调)
//  维护 pressed/clicked 语义:按下记录对象,抬起时若仍在同一对象则 Clicked
//  按下命中可聚焦对象时,切换焦点(旧焦点收 FocusLost,新焦点收 FocusGot)
void YMGUI_Event_Pointer(GYCTX ctx, GYcoord x, GYcoord y, uint8 pressed);

//处理一次按键(由 HAL 的 YMGUI_Inject_Key 转调):派发 GY_EVENT_Key 给焦点对象
void YMGUI_Event_Key(GYCTX ctx, uint32 key);

//处理一次滚轮:x,y 为指针屏幕坐标,delta_x/y 为平台归一化后的滚动增量。
//命中对象收到 GY_EVENT_Wheel,增量保存在 ctx->wheel_x/y。
void YMGUI_Event_Wheel(GYCTX ctx, GYcoord x, GYcoord y, int32 delta_x, int32 delta_y);

//推进上下文时钟:只向当前普通按下对象派 GY_EVENT_Tick。
void YMGUI_Event_Tick(GYCTX ctx, uint32 elapsed_ms);

//处理一次双击(由 HAL 的 YMGUI_Inject_DoubleClick 转调):命中测试后派 GY_EVENT_DoubleClicked。
//  先按普通指针语义确保对象已聚焦/光标已定位(内部走一次 press+release),再派双击。
void YMGUI_Event_DoubleClick(GYCTX ctx, GYcoord x, GYcoord y);

//取消当前指针捕获:清按下状态并派 ReleasedOff,不产生 Clicked。无捕获时幂等。
void YMGUI_Event_PointerCancel(GYCTX ctx);
//一次性上下文请求(右键短点击):命中测试后派 ContextRequested,不建立拖动捕获。
void YMGUI_Event_ContextRequest(GYCTX ctx, GYcoord x, GYcoord y);
//捕获式上下文手势:Begin 命中并派 Requested;Move/End 始终归起点对象;Cancel 异常结束。
void YMGUI_Event_ContextBegin(GYCTX ctx, GYcoord x, GYcoord y);
void YMGUI_Event_ContextMove(GYCTX ctx, GYcoord x, GYcoord y);
void YMGUI_Event_ContextEnd(GYCTX ctx, GYcoord x, GYcoord y);
void YMGUI_Event_ContextCancel(GYCTX ctx);

//设置焦点对象(NULL 表示清焦点)。切换时派发 FocusLost/FocusGot
void YMGUI_SetFocus(GYCTX ctx, GYOBJ obj);

//把焦点轮转到对象树里"下一个"可聚焦对象(前序遍历顺序,跳过 Hidden 子树)。
//  到末尾回卷到第一个;当前无焦点则聚焦第一个;树里没有可聚焦对象则不动。
//  Tab 键(GY_KEY_TAB)在 Event_Key 里调用它。
void YMGUI_FocusNext(GYCTX ctx);

#endif // !YMGUI_EVENT_H
