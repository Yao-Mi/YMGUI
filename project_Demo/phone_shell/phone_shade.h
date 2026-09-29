#ifndef PHONE_SHADE_H
#define PHONE_SHADE_H
#include <stdint.h>
#include "YMGUI_Obj.h"
/* 应用级控制中心：不访问 OS 网络、音量或通知服务。 */
void PhoneShade_Init(GYCTX context);
void PhoneShade_Update(int enabled);
void PhoneShade_Open(void);
void PhoneShade_Capture(void);
int PhoneShade_Close(int animate);
int PhoneShade_Visible(void);
void PhoneShade_SetWifi(int on);
int PhoneShade_GetWifi(void);
void PhoneShade_Sync(void);
void PhoneShade_Shutdown(void);
/* 只读诊断入口，不供 APP 业务访问内部控件。 */
intptr_t PhoneShade_Inspect(const char* name);
#endif
