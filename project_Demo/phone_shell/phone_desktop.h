#ifndef PHONE_DESKTOP_H
#define PHONE_DESKTOP_H
#include "phone_ui.h"
void PhoneDesktop_Init(GYCTX ctx, GYOBJ strip, GYOBJ* icons, int* page,
					   void (*select_page)(int, int), void (*removed)(int));
void PhoneDesktop_BuildOverlay(void);
int PhoneDesktop_Index(int id);
int PhoneDesktop_Hit(int x, int y);
void PhoneDesktop_Context(GYEvent event);
void PhoneDesktop_Tick(uint32 elapsed);
int PhoneDesktop_Busy(void);
int PhoneDesktop_Close(void);
/* 只读测试入口 */
intptr_t PhoneDesktop_Inspect(const char* name);
#endif
