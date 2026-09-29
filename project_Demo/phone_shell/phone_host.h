#ifndef PHONE_HOST_H
#define PHONE_HOST_H

#include "phone_app.h"

/* APP 只能经这些服务操作桌面，不引用桌面全局变量或其他 APP 的控件。 */
int PhoneHost_Open(int id, const char* argument);
void PhoneHost_Close(void);
void PhoneHost_Power(void);
void PhoneHost_Notice(const char* title, const char* message);
void PhoneHost_SetTheme(int alternate);
void PhoneHost_SetBrightness(int value);
int PhoneHost_GetBrightness(void);
void PhoneHost_SetWifi(int on);
int PhoneHost_GetWifi(void);
/* 同步复制未调暗的当前手机画面；调用方提供 320*480 native 像素。 */
void PhoneHost_Capture(GYpx* pixels);
void PhoneHost_MusicChanged(int playing);

#endif
