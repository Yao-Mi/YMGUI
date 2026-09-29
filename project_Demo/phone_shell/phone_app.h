#ifndef PHONE_APP_H
#define PHONE_APP_H

#include "YMGUI_Obj.h"
#include <stdint.h>

#ifndef PHONE_APP_CATALOG
#define PHONE_APP_CATALOG "phone_app_catalog.def"
#endif

typedef enum
{
#define PHONE_APP(id, file) id,
#include PHONE_APP_CATALOG
#undef PHONE_APP
	APP_COUNT
} PhoneAppId;

/* 单 context、静态编译应用。控件树归宿主管理，业务状态由各模块私有保存。 */
typedef struct
{
	const char* key;
	const char* title;
	GYcolor color;
	uint8 icon;
	void (*create)(GYOBJ view);
	void (*show)(const char* argument); /* 参数只在本次调用有效；需要保留时自行复制。 */
	void (*hide)(void);					/* 回桌面/切应用，不等于清理后台。 */
	void (*tick)(uint32 elapsed);		/* 只派发给运行中的应用，包括后台。 */
	int (*back)(void);					/* 非零表示已处理返回。 */
	void (*close)(void);				/* 停止活动，保留本次会话内容。 */
	void (*destroy)(void);				/* 退出前释放模块自己的非控件资源。 */
	int (*command)(const char* name, const char* argument);
	int (*query)(const char* name);
	/* 只供回归测试读取句柄/数值，业务通信不得使用。 */
	intptr_t (*inspect)(const char* name, int index);
} PhoneApp;

const PhoneApp* PhoneApps_Get(int id);
int PhoneApps_Find(const char* key);
int PhoneApps_Validate(void);
void PhoneApps_Create(int id, GYOBJ view);
void PhoneApps_SyncRunning(const int* running);
void PhoneApps_Show(int id, const char* argument);
void PhoneApps_Hide(void);
void PhoneApps_Tick(uint32 elapsed);
int PhoneApps_Back(void);
void PhoneApps_CloseAll(void);
void PhoneApps_Destroy(void);
int PhoneApps_Command(int id, const char* name, const char* argument);
int PhoneApps_Query(int id, const char* name);
intptr_t PhoneApps_Inspect(int id, const char* name, int index);

#endif
