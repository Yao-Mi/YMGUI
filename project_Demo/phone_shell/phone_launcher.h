#ifndef PHONE_LAUNCHER_H
#define PHONE_LAUNCHER_H

/* Startup layout for the 320x480 demo, not a general responsive layout engine. */
#define PHONE_LAUNCHER_FIRST_CAPACITY 4
#define PHONE_LAUNCHER_PAGE_CAPACITY 8
#define PHONE_LAUNCHER_PAGE_COUNT(count)                                                                                    \
	(1 + ((count) > PHONE_LAUNCHER_FIRST_CAPACITY                                                                           \
			  ? ((count) - PHONE_LAUNCHER_FIRST_CAPACITY + PHONE_LAUNCHER_PAGE_CAPACITY - 1) / PHONE_LAUNCHER_PAGE_CAPACITY \
			  : 0))

typedef struct
{
	int page, x, y;
} PhoneLauncherSlot;

/* Return 0 for an invalid index. Positions are relative to the page container. */
int PhoneLauncher_Place(int index, int count, PhoneLauncherSlot* slot);

/* 顺序/隐藏是桌面会话状态，不改变 APP 注册表或应用 ID。存储由调用者提供。 */
typedef struct
{
	int* order;
	int count, capacity;
} PhoneLauncherModel;
void PhoneLauncher_Reset(PhoneLauncherModel* model, int* storage, int capacity);
int PhoneLauncher_Index(const PhoneLauncherModel* model, int app_id);
int PhoneLauncher_Move(PhoneLauncherModel* model, int app_id, int index);
int PhoneLauncher_Remove(PhoneLauncherModel* model, int app_id);

#endif
