#ifndef YMGUI_BIND_H
#define YMGUI_BIND_H

#include "YMGUI_PubType.h"
#include "YMGUI_Obj.h"
#include "YMGUI_State.h"

//===========================================================================
// 控件级数据绑定(每控件一个 *_Bind,对标 LVGL v9 lv_xxx_bind_value)
//   符号直连:绑定期给 &subject,运行时纯指针,无路径无查表。
//   解绑不需显式调用——控件 YMGUI_Free_ObjFree 时自动摘链。
//===========================================================================

//标签绑定(单向:subject→文本)。按 subject 类型自动格式化:
//  Int/Fixed→数字串,Bool→"0"/"1",Str→直接显示。
void YMGUI_Label_Bind(GYOBJ label, GYSUBJECT s);

//滑块绑定(双向:subject↔value)。拖动写回 subject,subject 变更刷新滑块。
//  注意:会占用滑块的 changed 回调,绑定后勿再单独 SetChanged。
void YMGUI_Slider_Bind(GYOBJ slider, GYSUBJECT s);

//开关绑定(双向:subject↔on)。点击写回 bool,subject 变更刷新开关。占用 changed 回调。
void YMGUI_Switch_Bind(GYOBJ sw, GYSUBJECT s);

//复选框绑定(双向:subject↔checked)。占用 changed 回调。
void YMGUI_Checkbox_Bind(GYOBJ cb, GYSUBJECT s);

//进度条绑定(单向:subject→value)。只显示,无写回。
void YMGUI_Bar_Bind(GYOBJ bar, GYSUBJECT s);

//环形进度绑定(单向:subject→value)。只显示,无写回。
void YMGUI_Arc_Bind(GYOBJ arc, GYSUBJECT s);

//仪表盘绑定(单向:subject→value)。只显示,无写回。
void YMGUI_Meter_Bind(GYOBJ meter, GYSUBJECT s);

//文本框绑定(双向:subject↔text)。用户编辑写回 str(指向内部缓冲),subject 变更刷新文本。
//  占用 changed 回调。注意:subject 会指向文本框内部缓冲,该缓冲随文本框存活。
void YMGUI_TextInput_Bind(GYOBJ ti, GYSUBJECT s);

#endif // !YMGUI_BIND_H
