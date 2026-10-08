#ifndef YAOMI_PHONE_IME_H
#define YAOMI_PHONE_IME_H
#include "YMGUI_TextInput.h"
#include "YMGUI_EditView.h"

/* 应用级单实例服务；控件的原生私有数据、事件与析构均保留。 */
#if YAOMI_IME
int PhoneIME_Init(GYCTX ctx);
GYOBJ PhoneIME_Keyboard(void);
int PhoneIME_IsKey(GYOBJ obj);
void PhoneIME_Update(int enabled);
void PhoneIME_Shutdown(void);
int PhoneIME_Hide(void);
int PhoneIME_Visible(void);
GYOBJ PhoneIME_Target(void);
GYOBJ PhoneIME_TextInput(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h, size_t capacity);
GYOBJ PhoneIME_EditView(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h, size_t capacity);
/* 手机应用统一创建入口：以后新增单行/多行输入框也默认接入。 */
#ifdef PHONE_IME_AUTO_INPUTS
#define YMGUI_Creat_TextInput_Creat PhoneIME_TextInput
#define YMGUI_Creat_EditView_Creat PhoneIME_EditView
#endif
#else
static inline int PhoneIME_Init(GYCTX ctx)
{
	(void)ctx;
	return 1;
}
static inline GYOBJ PhoneIME_Keyboard(void)
{
	return NULL;
}
static inline int PhoneIME_IsKey(GYOBJ obj)
{
	(void)obj;
	return 0;
}
static inline void PhoneIME_Update(int enabled)
{
	(void)enabled;
}
static inline void PhoneIME_Shutdown(void)
{
}
static inline int PhoneIME_Hide(void)
{
	return 0;
}
static inline int PhoneIME_Visible(void)
{
	return 0;
}
static inline GYOBJ PhoneIME_Target(void)
{
	return NULL;
}
#endif
#endif
