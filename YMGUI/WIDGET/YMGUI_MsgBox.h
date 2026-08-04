#ifndef YMGUI_MSGBOX_H
#define YMGUI_MSGBOX_H

#include "YMGUI_PubType.h"
#include "YMGUI_PubDefine.h"
#include "YMGUI_Obj.h"

#if YMGUI_MSGBOX

//===========================================================================
// 模态对话框 / 消息框(MsgBox):居中卡片 + 全屏遮罩,输入模态。
//
//   "阻塞弹窗"在保留模式 + 单事件循环里 = 输入模态(input-modal),不是线程阻塞:
//   全屏 backdrop 挂 top_layer(渲染最上、命中最先),吞掉所有落在卡片外的点击 →
//   底层 UI 被锁死;只有点卡片上的按钮才隐藏模态解锁。对标 LVGL lv_msgbox(异步回调式)。
//
//   结构(全挂 top_layer,root 子树里无所有者 → 析构走 CtxFree 的 top_layer 兜底,无 double-free):
//     backdrop(全屏,半透明变暗 + 吞点击) → card(居中,标题/正文多行/底部 N 个按钮)。
//
//   生命周期(沿用 Dropdown 铁律):关闭只 SetHidden(不释放——按钮回调仍在其事件派发中);
//   重配置按钮在 Show()(安全上下文,不在事件调用栈)里"先拆旧再建新",可反复复用不泄漏。
//
//   可裁减:YMGUI_MSGBOX(PubDefine.h,默认 1)。库核心不依赖 → =0 整控件裁空、无悬空符号。
//===========================================================================

#ifndef GY_MSGBOX_TITLE_MAX
#define GY_MSGBOX_TITLE_MAX 48  //标题上限(含 '\0'),UTF-8 字节
#endif
#ifndef GY_MSGBOX_TEXT_MAX
#define GY_MSGBOX_TEXT_MAX 256  //正文上限(含 '\0'),支持 '\n' 多行
#endif
#ifndef GY_MSGBOX_MAX_BTN
#define GY_MSGBOX_MAX_BTN 4     //按钮个数上限
#endif

//按钮点击回调:mb 为对话框对象,index 为被点按钮下标(0..N-1)。回调返回后模态自动隐藏
typedef void (*GYmsgbox_btn_cb)(GYOBJ mb, int index);

//创建模态对话框(挂 ctx 的 top_layer;初始隐藏,需 Show 才显示)。失败返回 NULL
GYOBJ YMGUI_Creat_MsgBox_Creat(GYCTX ctx);

//设标题(拷贝,截断到上限)
void  YMGUI_MsgBox_SetTitle(GYOBJ mb, const char* title);
//设正文(拷贝;'\n' 断多行,截断到上限)
void  YMGUI_MsgBox_SetText(GYOBJ mb, const char* text);
//追加一个按钮(label 拷贝,cb 可为 NULL)。返回按钮下标,满返 -1。下次 Show 生效
int   YMGUI_MsgBox_AddButton(GYOBJ mb, const char* label, GYmsgbox_btn_cb cb);
//清空所有按钮(下次 Show 生效)
void  YMGUI_MsgBox_ClearButtons(GYOBJ mb);

//显示模态:按当前标题/正文/按钮重建卡片内容并居中,置顶显示(锁死底层)
void  YMGUI_MsgBox_Show(GYOBJ mb);
//主动关闭(只隐藏,内容留到下次 Show 重建或析构时释放)
void  YMGUI_MsgBox_Close(GYOBJ mb);
//当前是否显示(模态锁死中)
uint8 YMGUI_MsgBox_IsShown(GYOBJ mb);

//设颜色:遮罩色(含 alpha,变暗底层)/ 卡片底 / 卡片边框 / 标题字 / 正文字
void  YMGUI_MsgBox_SetColors(GYOBJ mb, GYcolor backdrop, GYcolor card,
                             GYcolor border, GYcolor title, GYcolor text);

#endif // YMGUI_MSGBOX

#endif // !YMGUI_MSGBOX_H
