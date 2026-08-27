#include "YMGUI_Joystick.h"

#if YMGUI_JOYSTICK

#include "YMGUI_Invalidate.h"
#include "YMGUI_DrawArc.h"
#include "YMGUI_DrawLine.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Debug.h"

typedef struct
{
	int16 value_x;
	int16 value_y;
	uint16 deadzone;
	uint8 auto_center;
	uint8 active;
	GYjoystick_changed_cb changed_cb;
	GYjoystick_released_cb released_cb;
	GYjoystick_draw_cb custom_draw_cb;
}GYjoystick_data;

static uint32 isqrt32(uint32 value)
{
	uint32 result = 0;
	uint32 bit = (uint32)1 << 30;
	while (bit > value)
		bit >>= 2;
	while (bit != 0)
	{
		if (value >= result + bit)
		{
			value -= result + bit;
			result = (result >> 1) + bit;
		}
		else
			result >>= 1;
		bit >>= 2;
	}
	return result;
}

static GYcoord joystickRadius(const GYrect* area)
{
	GYcoord radius = GYMin(area->w, area->h) / 2 - 2;
	return radius > 2 ? radius : 2;
}

static GYcoord joystickTravel(const GYrect* area)
{
	GYcoord radius = joystickRadius(area);
	GYcoord knob = radius / 3;
	if (knob < 2) knob = 2;
	return radius > knob ? radius - knob : 1;
}

static void clampValue(int32* x, int32* y)
{
	uint32 ax = (uint32)(*x < 0 ? -*x : *x);
	uint32 ay = (uint32)(*y < 0 ? -*y : *y);
	uint32 length = isqrt32(ax * ax + ay * ay);
	if (length > GY_JOYSTICK_VALUE_MAX)
	{
		*x = *x * GY_JOYSTICK_VALUE_MAX / (int32)length;
		*y = *y * GY_JOYSTICK_VALUE_MAX / (int32)length;
	}
}

static uint8 setValue(GYOBJ joystick, GYjoystick_data* d, int32 x, int32 y, uint8 notify)
{
	clampValue(&x, &y);
	if (d->value_x == x && d->value_y == y)
		return 0;
	d->value_x = (int16)x;
	d->value_y = (int16)y;
	YMGUI_Obj_Invalidate(joystick);
	if (notify && d->changed_cb != NULL)
		d->changed_cb(joystick, d->value_x, d->value_y);
	return 1;
}

static void applyPointer(GYOBJ joystick)
{
	GYjoystick_data* d = (GYjoystick_data*)joystick->user_data;
	GYrect abs;
	YMGUI_Obj_GetAbsArea(joystick, &abs);
	GYcoord travel = joystickTravel(&abs);
	int32 dx = (int32)joystick->ctx->point_x - (abs.x + abs.w / 2);
	int32 dy = (int32)joystick->ctx->point_y - (abs.y + abs.h / 2);
	uint32 ax = (uint32)(dx < 0 ? -dx : dx);
	uint32 ay = (uint32)(dy < 0 ? -dy : dy);
	uint32 distance = isqrt32(ax * ax + ay * ay);
	int32 value_x = dx * GY_JOYSTICK_VALUE_MAX / travel;
	int32 value_y = -dy * GY_JOYSTICK_VALUE_MAX / travel;
	clampValue(&value_x, &value_y);
	if (distance * GY_JOYSTICK_VALUE_MAX <= (uint32)d->deadzone * travel)
	{
		value_x = 0;
		value_y = 0;
	}
	setValue(joystick, d, value_x, value_y, 1);
}

static void defaultDraw(GYOBJ joystick, GYSURFACE surface, const GYrect* abs)
{
	GYjoystick_data* d = (GYjoystick_data*)joystick->user_data;
	GYcoord cx = abs->x + abs->w / 2;
	GYcoord cy = abs->y + abs->h / 2;
	GYcoord radius = joystickRadius(abs);
	GYcoord knob_radius = radius / 3;
	if (knob_radius < 1) knob_radius = 1;
	GYcoord inner_radius = radius > 3 ? radius - 3 : radius;
	GYcoord travel = joystickTravel(abs);
	GYcoord knob_x = cx + (GYcoord)((int32)d->value_x * travel / GY_JOYSTICK_VALUE_MAX);
	GYcoord knob_y = cy - (GYcoord)((int32)d->value_y * travel / GY_JOYSTICK_VALUE_MAX);
	GYcolor knob = d->active ? GY_ARGB(0xFF, 0xE8, 0xA8, 0x40) : GY_ARGB(0xFF, 0x48, 0xB8, 0xAC);

	YMGUI_Draw_CircleFill(surface, cx, cy, radius, GY_ARGB(0xFF, 0x30, 0x98, 0x9E));
	YMGUI_Draw_CircleFill(surface, cx, cy, inner_radius, GY_ARGB(0xFF, 0x20, 0x2B, 0x31));
	YMGUI_Draw_Line(surface, cx - travel, cy, cx + travel, cy, GY_ARGB(0xFF, 0x50, 0x60, 0x66));
	YMGUI_Draw_Line(surface, cx, cy - travel, cx, cy + travel, GY_ARGB(0xFF, 0x50, 0x60, 0x66));
	YMGUI_Draw_CircleFill(surface, knob_x + 2, knob_y + 2, knob_radius, GY_ARGB(0xFF, 0x0C, 0x12, 0x16));
	YMGUI_Draw_CircleFill(surface, knob_x, knob_y, knob_radius, knob);
}

static void joystickDrawCb(GYOBJ joystick, GYSURFACE surface, const GYrect* abs)
{
	GYjoystick_data* d = (GYjoystick_data*)joystick->user_data;
	if (d->custom_draw_cb != NULL)
		d->custom_draw_cb(joystick, surface, abs);
	else
		defaultDraw(joystick, surface, abs);
}

static void releaseJoystick(GYOBJ joystick)
{
	GYjoystick_data* d = (GYjoystick_data*)joystick->user_data;
	int16 final_x = d->value_x;
	int16 final_y = d->value_y;
	d->active = 0;
	if (d->auto_center)
		setValue(joystick, d, 0, 0, 1);
	YMGUI_Obj_Invalidate(joystick);
	if (d->released_cb != NULL)
		d->released_cb(joystick, final_x, final_y);
}

static void joystickEventCb(GYOBJ joystick, GYEvent event)
{
	GYjoystick_data* d = (GYjoystick_data*)joystick->user_data;
	switch (event)
	{
	case GY_EVENT_Pressed:
	case GY_EVENT_Pressing:
	case GY_EVENT_ContextRequested:
	case GY_EVENT_ContextDragging:
		d->active = 1;
		applyPointer(joystick);
		YMGUI_Obj_Invalidate(joystick);
		break;
	case GY_EVENT_Released:
	case GY_EVENT_ReleasedOff:
	case GY_EVENT_ContextReleased:
	case GY_EVENT_ContextCancelled:
		releaseJoystick(joystick);
		break;
	default:
		break;
	}
}

static void joystickFreeCb(GYOBJ joystick)
{
	if (joystick->user_data != NULL)
	{
		GY_free0(joystick->user_data);
		joystick->user_data = NULL;
	}
}

GYOBJ YMGUI_Creat_Joystick_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h)
{
	GYOBJ joystick = YMGUI_Creat_Obj_Creat(parent, x, y, w, h);
	gy_assert(joystick);
	if (joystick == NULL)
		return NULL;
	GYjoystick_data* d = (GYjoystick_data*)GY_malloc0(sizeof(GYjoystick_data));
	gy_assert(d);
	gy_log_explain(d == NULL, GY_LOG_Mem0, "摇杆数据内存申请失败");
	if (d == NULL) { YMGUI_Free_ObjFree(joystick); return NULL; }
	GY_memset(d, 0, sizeof(GYjoystick_data));
	d->deadzone = 8;
	d->auto_center = 1;
	joystick->type = GY_OBJ_Base;
	joystick->user_data = d;
	joystick->draw_cb = joystickDrawCb;
	joystick->event_cb = joystickEventCb;
	joystick->free_cb = joystickFreeCb;
	YMGUI_Obj_Invalidate(joystick);
	return joystick;
}

void YMGUI_Joystick_SetValue(GYOBJ joystick, int16 x, int16 y)
{
	gy_assert(joystick && joystick->user_data);
	if (joystick == NULL || joystick->user_data == NULL) return;
	setValue(joystick, (GYjoystick_data*)joystick->user_data, x, y, 0);
}

void YMGUI_Joystick_GetValue(GYOBJ joystick, int16* x, int16* y)
{
	gy_assert(joystick && joystick->user_data);
	if (joystick == NULL || joystick->user_data == NULL) return;
	GYjoystick_data* d = (GYjoystick_data*)joystick->user_data;
	if (x != NULL) *x = d->value_x;
	if (y != NULL) *y = d->value_y;
}

void YMGUI_Joystick_SetDeadzone(GYOBJ joystick, uint16 deadzone)
{
	gy_assert(joystick && joystick->user_data);
	if (joystick == NULL || joystick->user_data == NULL) return;
	GYjoystick_data* d = (GYjoystick_data*)joystick->user_data;
	d->deadzone = deadzone <= GY_JOYSTICK_VALUE_MAX ? deadzone : GY_JOYSTICK_VALUE_MAX;
}

uint16 YMGUI_Joystick_GetDeadzone(GYOBJ joystick)
{
	gy_assert(joystick && joystick->user_data);
	return (joystick != NULL && joystick->user_data != NULL)
		? ((GYjoystick_data*)joystick->user_data)->deadzone : 0;
}

void YMGUI_Joystick_SetAutoCenter(GYOBJ joystick, uint8 enabled)
{
	gy_assert(joystick && joystick->user_data);
	if (joystick == NULL || joystick->user_data == NULL) return;
	GYjoystick_data* d = (GYjoystick_data*)joystick->user_data;
	uint8 next = enabled ? 1 : 0;
	if (d->auto_center == next)
		return;
	d->auto_center = next;
	//保持模式留下的值在切回自动模式时立即归中；拖动中则等本次松手。
	if (next && !d->active)
		setValue(joystick, d, 0, 0, 1);
}

uint8 YMGUI_Joystick_GetAutoCenter(GYOBJ joystick)
{
	gy_assert(joystick && joystick->user_data);
	return (joystick != NULL && joystick->user_data != NULL)
		? ((GYjoystick_data*)joystick->user_data)->auto_center : 0;
}

uint8 YMGUI_Joystick_IsActive(GYOBJ joystick)
{
	gy_assert(joystick && joystick->user_data);
	return (joystick != NULL && joystick->user_data != NULL)
		? ((GYjoystick_data*)joystick->user_data)->active : 0;
}

void YMGUI_Joystick_SetChangedCb(GYOBJ joystick, GYjoystick_changed_cb cb)
{
	gy_assert(joystick && joystick->user_data);
	if (joystick == NULL || joystick->user_data == NULL) return;
	((GYjoystick_data*)joystick->user_data)->changed_cb = cb;
}

void YMGUI_Joystick_SetReleasedCb(GYOBJ joystick, GYjoystick_released_cb cb)
{
	gy_assert(joystick && joystick->user_data);
	if (joystick == NULL || joystick->user_data == NULL) return;
	((GYjoystick_data*)joystick->user_data)->released_cb = cb;
}

void YMGUI_Joystick_SetDrawCb(GYOBJ joystick, GYjoystick_draw_cb cb)
{
	gy_assert(joystick && joystick->user_data);
	if (joystick == NULL || joystick->user_data == NULL) return;
	((GYjoystick_data*)joystick->user_data)->custom_draw_cb = cb;
	YMGUI_Obj_Invalidate(joystick);
}

#endif // YMGUI_JOYSTICK
