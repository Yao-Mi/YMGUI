#ifndef PHONE_QUICK_H
#define PHONE_QUICK_H
#include "YMGUI_Obj.h"
#include "YMGUI_Surface.h"
#define PHONE_QUICK_CAPACITY 32
typedef struct
{
	const char* key;
	const char* title;
	int (*active)(void); /* NULL 表示一次性操作，而非开关。 */
	void (*activate)(void);
	void (*draw_icon)(GYSURFACE surface, int x, int y, GYcolor background);
} PhoneQuick;
/* 描述符须长寿命；按注册顺序排列。初始化面板后锁定，不支持运行期卸载。 */
int PhoneQuick_Register(const PhoneQuick* feature);
int PhoneQuick_Count(void);
const PhoneQuick* PhoneQuick_Get(int index);
const PhoneQuick* PhoneQuick_Find(const char* key);
void PhoneQuick_Lock(void);
/* 每页 8 个，4 列 2 行；返回页号，非法索引返回 -1。 */
int PhoneQuick_Place(int index, GYrect* slot);
int PhoneQuick_RegisterDefaults(void);
#endif
