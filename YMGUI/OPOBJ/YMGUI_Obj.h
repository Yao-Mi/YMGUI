#ifndef YMGUI_OBJ_H
#define YMGUI_OBJ_H

#include "YMGUI_PubType.h"
#include "YMGUI_PubDefine.h" //GY_INV_MAX
#include "YMGUI_Surface.h"

//===========================================================================
// 对象树核心类型(保留模式)。控件 = GYobj + 两个回调(draw_cb / event_cb)
//   坐标 x,y 相对父对象;遍历时累加父原点得到屏幕绝对坐标
//===========================================================================

//对象类型(裁剪/命中/调试用)
typedef enum
{
	GY_OBJ_Base = 0, //基础容器
	GY_OBJ_Button,   //按钮
	GY_OBJ_Label,    //文本标签
	GY_OBJ_List,     //列表
	GY_OBJ_Dropdown, //下拉框(弹出菜单挂 top_layer)
	GY_OBJ_Tabview,  //标签页容器(tab bar 切页,SetHidden 显隐页)
}GYObjType;

//事件类型
typedef enum
{
	GY_EVENT_Pressed = 0, //指针在对象上按下
	GY_EVENT_Pressing,    //指针按住并移动(拖动中,派发给已捕获对象)
	GY_EVENT_Released,    //指针抬起(仍在对象上)
	GY_EVENT_ReleasedOff, //指针抬起(已移出对象)
	GY_EVENT_Clicked,     //按下+抬起都在对象上 = 点击
	GY_EVENT_DoubleClicked,//同一对象上快速两次点击(HAL 双击注入,派给命中对象;编辑器里=选词)
	GY_EVENT_FocusGot,    //获得焦点
	GY_EVENT_FocusLost,   //失去焦点
	GY_EVENT_Key,         //键盘输入(焦点对象收,键值在 ctx->last_key)
	GY_EVENT_ContextRequested,//请求上下文操作(鼠标右键/触摸长按等平台等效输入)
	GY_EVENT_ContextDragging, //上下文手势拖动中(派给起点捕获对象)
	GY_EVENT_ContextReleased, //上下文手势正常结束
	GY_EVENT_ContextCancelled,//上下文手势被后台/失焦等取消
}GYEvent;

//对象状态位(可组合)
#define GY_STATE_Default   0x00
#define GY_STATE_Pressed   0x01
#define GY_STATE_Focused   0x02
#define GY_STATE_Hidden    0x04
#define GY_STATE_Focusable    0x08  //可获得键盘焦点(点击时获焦)
#define GY_STATE_ClipChildren 0x10  //子对象绘制裁剪到本对象区域(滚动容器用)
#define GY_STATE_Editing      0x20  //编辑态(焦点两级:选中态仅高亮/Tab 轮转,编辑态吃 Tab 插空格/Enter 出;
                                    //点击进编辑,Tab 轮转落选择态,Enter/打字进编辑)

struct GYobj;
struct GYctx;
typedef struct GYobj* GYOBJ;
typedef struct GYctx* GYCTX;

//绘制回调:把自己画进 surface(屏幕绝对坐标 abs 已算好)
typedef void (*GYobj_draw_cb)(struct GYobj* obj, GYSURFACE s, const GYrect* abs);
//事件回调:收到事件时被调用(用户可覆盖以响应交互)
typedef void (*GYobj_event_cb)(struct GYobj* obj, GYEvent e);
//析构回调:释放对象前调用,让控件清理自己的 user_data(NULL 表示无需清理)
typedef void (*GYobj_free_cb)(struct GYobj* obj);

//对象(值类型小写,指针 typedef 全大写)
typedef struct GYobj
{
	GYObjType type;      //对象类型
	GYrect    area;      //相对父对象的位置+尺寸
	GYcoord   scroll_x, scroll_y;//内容滚动偏移(子对象绘制/命中时减去)
	uint8     state;     //状态位组合
	GYcolor   bg_color;  //背景色(基础绘制用)

	struct GYobj* parent;     //父对象(root 为 NULL)
	struct GYobj* child_head; //子链表头
	struct GYobj* sibling;    //下一个兄弟
	struct GYctx* ctx;        //所属上下文

	GYobj_draw_cb  draw_cb;   //绘制回调
	GYobj_event_cb event_cb;  //事件回调
	GYobj_free_cb  free_cb;   //析构回调(清理 user_data)
	void*          user_data; //用户数据(控件私有数据)
	void*          bind_data; //数据绑定用(observer 节点,与 user_data 分开;见 STATE/)
}GYobj;

//上下文:一棵对象树 + 显示 + 交互状态(无全局状态,贯穿始终)
typedef struct GYctx
{
	struct GYobj* root;    //根对象(通常铺满全屏)
	//顶层:全屏透明容器,parent=NULL、无 draw_cb。渲染在 root 之后(总在最上层),
	//命中在 root 之前。弹出层(Dropdown 菜单/未来 Tooltip 等)挂这里以跨子树置顶、
	//不被任何父的 ClipChildren 裁掉。
	struct GYobj* top_layer;
	void*  disp;           //GYDISP(void* 避免 OPOBJ 依赖 HAL)
	//交互状态
	struct GYobj* pressed_obj; //当前普通指针按下的对象
	struct GYobj* context_obj; //当前上下文拖动捕获对象(右键/长按,与 pressed 独立)
	struct GYobj* focus_obj;   //当前焦点对象
	GYcoord point_x, point_y;  //当前指针位置(屏幕坐标,控件事件里可读)
	uint8   point_pressed;     //指针当前是否按下
	uint32  last_key;          //最近一次按键键值(焦点对象在 GY_EVENT_Key 里读)
	uint8   key_handled;       //焦点控件在 GY_EVENT_Key 里置 1 表示"已消费"此键(如 EditView 吃掉 Tab)
	                           //Event_Key 派发前清 0,派发后据此决定是否轮转焦点(Tab 未被吃才切焦点)
	//失效/脏区:非重叠矩形列表(GUI 层维护,见 YMGUI_Invalidate)
	GYrect inv_areas[GY_INV_MAX]; //脏矩形列表(屏幕坐标,互不重叠)
	uint8  inv_cnt;               //当前脏矩形数量
	uint8  destroying;            //上下文销毁中:不再累计无下一帧可消费的脏区
}GYctx;

//---- 生命周期(Creat/Free 成对,Free 树级联) ----
GYCTX YMGUI_Creat_Ctx_Creat(void* disp, GYcoord w, GYcoord h);//创建上下文+根对象
void  YMGUI_Free_CtxFree(GYCTX ctx);                          //释放上下文(级联释放整棵树)

GYOBJ YMGUI_Creat_Obj_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h);//创建基础对象并挂到 parent
void  YMGUI_Free_ObjFree(GYOBJ obj);                          //释放对象(递归级联释放子节点)

//---- 顶层/弹出层 ----
GYOBJ YMGUI_Ctx_GetTopLayer(GYCTX ctx);                       //取顶层容器(弹出层挂它)

//---- 树/坐标辅助 ----
void  YMGUI_Obj_GetAbsArea(GYOBJ obj, GYRECT abs);            //算对象的屏幕绝对矩形(累加父原点)
void  YMGUI_Obj_SetBgColor(GYOBJ obj, GYcolor color);         //设背景色(自动标脏)
void  YMGUI_Obj_SetHidden(GYOBJ obj, uint8 hidden);           //显示/隐藏(切换 Hidden 位并标脏)

#endif // !YMGUI_OBJ_H
