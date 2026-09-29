#include "YMGUI_Anim.h"
#include "YMGUI_Debug.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Mem.h"

#if YMGUI_ANIM

#define GY_ANIM_ONE 65536

typedef enum
{
	ANIM_MOVE,
	ANIM_RESIZE,
	ANIM_COLOR,
	ANIM_SCROLL,
	ANIM_VALUE
} AnimKind;

typedef struct GYanim_node
{
	struct GYanim_node* next;
	GYANIM id;
	AnimKind kind;
	GYAnimEase ease;
	GYOBJ owner;
	uint32 elapsed_ms;
	uint32 delay_ms;
	uint32 duration_ms;
	uint8 dead;
	uint8 cancelled;
	GYanim_done_cb done_cb;
	void* done_user;
	union
	{
		struct
		{
			GYcoord x0, y0, x1, y1;
		} pair;
		struct
		{
			GYcolor from, to;
		} color;
		struct
		{
			int32 from, to;
			GYanim_value_cb callback;
			void* user;
		} value;
	} data;
} GYanim_node;

typedef struct
{
	GYanim_node* head;
	GYANIM next_id;
	GYanim_rate_cb rate_cb;
	void* rate_user;
	uint8 ticking;
} GYanim_manager;

static int32 mulFixed(int32 a, int32 b)
{
	return (int32)(((int64)a * b) / GY_ANIM_ONE);
}

static int32 smoothFixed(int32 t)
{
	int32 t2 = mulFixed(t, t);
	return mulFixed(t2, 3 * GY_ANIM_ONE - 2 * t);
}

static int32 defaultRate(GYAnimEase ease, int32 t)
{
	int32 inv;
	if (t < 0)
		t = 0;
	if (t > GY_ANIM_ONE)
		t = GY_ANIM_ONE;
	switch (ease)
	{
	case GY_ANIM_SMOOTH:
		return smoothFixed(t);
	case GY_ANIM_EASE_IN:
		return mulFixed(t, t);
	case GY_ANIM_EASE_OUT:
		inv = GY_ANIM_ONE - t;
		return GY_ANIM_ONE - mulFixed(inv, inv);
	case GY_ANIM_OVERSHOOT:
	{
		// easeOutBack，常量分别为 1.70158 / 2.70158 的 16.16 表示。
		int32 x = t - GY_ANIM_ONE;
		int32 x2 = mulFixed(x, x);
		int32 x3 = mulFixed(x2, x);
		return GY_ANIM_ONE + mulFixed(177051, x3) + mulFixed(111514, x2);
	}
	case GY_ANIM_THERE_AND_BACK:
		return (t <= GY_ANIM_ONE / 2) ? smoothFixed(t * 2) : smoothFixed((GY_ANIM_ONE - t) * 2);
	default:
		return t;
	}
}

static int32 lerpI32(int32 from, int32 to, int32 progress)
{
	int64 value = (int64)from + (((int64)to - from) * progress) / GY_ANIM_ONE;
	if (value > INT32_MAX)
		return INT32_MAX;
	if (value < INT32_MIN)
		return INT32_MIN;
	return (int32)value;
}

static GYcoord lerpCoord(GYcoord from, GYcoord to, int32 progress)
{
	int32 value = lerpI32(from, to, progress);
	if (sizeof(GYcoord) == sizeof(int16))
	{
		if (value > INT16_MAX) value = INT16_MAX;
		if (value < INT16_MIN) value = INT16_MIN;
	}
	return (GYcoord)value;
}

static GYanim_manager* getManager(GYCTX ctx, uint8 create)
{
	if (ctx == NULL)
		return NULL;
	GYanim_manager* manager = (GYanim_manager*)ctx->runtime_data;
	if (manager != NULL || !create)
		return manager;
	manager = (GYanim_manager*)GY_malloc0(sizeof(*manager));
	if (manager == NULL)
		return NULL;
	GY_memset(manager, 0, sizeof(*manager));
	manager->next_id = 1;
	ctx->runtime_data = manager;
	return manager;
}

static GYanim_node* findNode(GYanim_manager* manager, GYANIM id)
{
	for (GYanim_node* node = manager != NULL ? manager->head : NULL; node != NULL; node = node->next)
		if (node->id == id && !node->dead)
			return node;
	return NULL;
}

static void sweep(GYanim_manager* manager)
{
	if (manager == NULL || manager->ticking)
		return;
	for (;;)
	{
		GYanim_node** link = &manager->head;
		while (*link != NULL && !(*link)->dead)
			link = &(*link)->next;
		if (*link == NULL)
			break;
		GYanim_node* node = *link;
		*link = node->next;
		GYanim_done_cb callback = node->done_cb;
		void* user = node->done_user;
		GYANIM id = node->id;
		uint8 cancelled = node->cancelled;
		GY_free0(node);
		if (callback != NULL)
			callback(id, cancelled, user);
	}
}

static void cancelKind(GYanim_manager* manager, GYOBJ owner, AnimKind kind)
{
	for (GYanim_node* node = manager != NULL ? manager->head : NULL; node != NULL; node = node->next)
	{
		if (!node->dead && node->owner == owner && node->kind == kind)
		{
			node->dead = 1;
			node->cancelled = 1;
		}
	}
	sweep(manager);
}

static void managerCancelObject(void* data, GYOBJ obj)
{
	GYanim_manager* manager = (GYanim_manager*)data;
	for (GYanim_node* node = manager != NULL ? manager->head : NULL; node != NULL; node = node->next)
	{
		if (!node->dead && node->owner == obj)
		{
			node->dead = 1;
			node->cancelled = 1;
			// 对象析构不是业务取消事件；禁止完成回调重新给正在销毁的对象开动画。
			node->done_cb = NULL;
		}
	}
	sweep(manager);
}

static void managerFree(void* data)
{
	GYanim_manager* manager = (GYanim_manager*)data;
	if (manager == NULL)
		return;
	GYanim_node* node = manager->head;
	while (node != NULL)
	{
		GYanim_node* next = node->next;
		GY_free0(node);
		node = next;
	}
	GY_free0(manager);
}

static GYanim_node* createNode(GYCTX ctx, GYOBJ owner, AnimKind kind,
							   uint32 duration_ms, GYAnimEase ease)
{
	if (ctx == NULL || duration_ms == 0 || ease >= GY_ANIM_EASE_COUNT || ctx->destroying)
		return NULL;
	GYanim_manager* manager = getManager(ctx, 1);
	if (manager == NULL)
		return NULL;
	if (ctx->runtime_obj_free_cb == NULL)
	{
		ctx->runtime_obj_free_cb = managerCancelObject;
		ctx->runtime_ctx_free_cb = managerFree;
	}
	// 替换回调可能再创建动画，先延迟清扫到新节点完全初始化之后，避免重入 UAF。
	uint8 was_ticking = manager->ticking;
	manager->ticking = 1;
	if (kind != ANIM_VALUE)
		cancelKind(manager, owner, kind);
	manager->ticking = was_ticking;
	GYanim_node* node = (GYanim_node*)GY_malloc0(sizeof(*node));
	if (node == NULL)
		return NULL;
	GY_memset(node, 0, sizeof(*node));
	node->id = manager->next_id++;
	if (node->id == 0)
		node->id = manager->next_id++;
	node->kind = kind;
	node->ease = ease;
	node->owner = owner;
	node->duration_ms = duration_ms;
	node->next = manager->head;
	manager->head = node;
	return node;
}

static GYANIM finishNode(GYCTX ctx, GYanim_node* node)
{
	GYANIM id = node->id;
	sweep(getManager(ctx, 0));
	return id;
}

GYANIM YMGUI_Anim_MoveTo(GYOBJ obj, GYcoord x, GYcoord y, uint32 duration_ms, GYAnimEase ease)
{
	if (obj == NULL || obj->ctx == NULL)
		return 0;
	GYanim_node* node = createNode(obj->ctx, obj, ANIM_MOVE, duration_ms, ease);
	if (node == NULL)
		return 0;
	node->data.pair.x0 = obj->area.x;
	node->data.pair.y0 = obj->area.y;
	node->data.pair.x1 = x;
	node->data.pair.y1 = y;
	return finishNode(obj->ctx, node);
}

GYANIM YMGUI_Anim_ResizeTo(GYOBJ obj, GYcoord w, GYcoord h, uint32 duration_ms, GYAnimEase ease)
{
	if (obj == NULL || obj->ctx == NULL || w <= 0 || h <= 0)
		return 0;
	GYanim_node* node = createNode(obj->ctx, obj, ANIM_RESIZE, duration_ms, ease);
	if (node == NULL)
		return 0;
	node->data.pair.x0 = obj->area.w;
	node->data.pair.y0 = obj->area.h;
	node->data.pair.x1 = w;
	node->data.pair.y1 = h;
	return finishNode(obj->ctx, node);
}

GYANIM YMGUI_Anim_BgColorTo(GYOBJ obj, GYcolor color, uint32 duration_ms, GYAnimEase ease)
{
	if (obj == NULL || obj->ctx == NULL)
		return 0;
	GYanim_node* node = createNode(obj->ctx, obj, ANIM_COLOR, duration_ms, ease);
	if (node == NULL)
		return 0;
	node->data.color.from = obj->bg_color;
	node->data.color.to = color;
	return finishNode(obj->ctx, node);
}

GYANIM YMGUI_Anim_ScrollTo(GYOBJ obj, GYcoord x, GYcoord y, uint32 duration_ms, GYAnimEase ease)
{
	if (obj == NULL || obj->ctx == NULL)
		return 0;
	GYanim_node* node = createNode(obj->ctx, obj, ANIM_SCROLL, duration_ms, ease);
	if (node == NULL)
		return 0;
	node->data.pair.x0 = obj->scroll_x;
	node->data.pair.y0 = obj->scroll_y;
	node->data.pair.x1 = x;
	node->data.pair.y1 = y;
	return finishNode(obj->ctx, node);
}

GYANIM YMGUI_Anim_ValueTo(GYCTX ctx, GYOBJ owner, int32 from, int32 to,
						  uint32 duration_ms, GYAnimEase ease, GYanim_value_cb callback, void* user_data)
{
	if (callback == NULL || (owner != NULL && owner->ctx != ctx))
		return 0;
	GYanim_node* node = createNode(ctx, owner, ANIM_VALUE, duration_ms, ease);
	if (node == NULL)
		return 0;
	node->data.value.from = from;
	node->data.value.to = to;
	node->data.value.callback = callback;
	node->data.value.user = user_data;
	return finishNode(ctx, node);
}

uint8 YMGUI_Anim_SetDelay(GYCTX ctx, GYANIM animation, uint32 delay_ms)
{
	GYanim_node* node = findNode(getManager(ctx, 0), animation);
	if (node == NULL || node->elapsed_ms != 0)
		return 0;
	node->delay_ms = delay_ms;
	return 1;
}

uint8 YMGUI_Anim_SetDone(GYCTX ctx, GYANIM animation, GYanim_done_cb callback, void* user_data)
{
	GYanim_node* node = findNode(getManager(ctx, 0), animation);
	if (node == NULL)
		return 0;
	node->done_cb = callback;
	node->done_user = user_data;
	return 1;
}

uint8 YMGUI_Anim_Cancel(GYCTX ctx, GYANIM animation)
{
	GYanim_manager* manager = getManager(ctx, 0);
	GYanim_node* node = findNode(manager, animation);
	if (node == NULL)
		return 0;
	node->dead = 1;
	node->cancelled = 1;
	sweep(manager);
	return 1;
}

void YMGUI_Anim_CancelObject(GYOBJ obj)
{
	if (obj != NULL && obj->ctx != NULL)
		managerCancelObject(obj->ctx->runtime_data, obj);
}

void YMGUI_Anim_CancelAll(GYCTX ctx)
{
	GYanim_manager* manager = getManager(ctx, 0);
	for (GYanim_node* node = manager != NULL ? manager->head : NULL; node != NULL; node = node->next)
	{
		node->dead = 1;
		node->cancelled = 1;
	}
	sweep(manager);
}

uint16 YMGUI_Anim_ActiveCount(GYCTX ctx)
{
	uint16 count = 0;
	GYanim_manager* manager = getManager(ctx, 0);
	for (GYanim_node* node = manager != NULL ? manager->head : NULL; node != NULL; node = node->next)
		if (!node->dead && count != UINT16_MAX)
			count++;
	return count;
}

static uint8 colorPart(GYcolor color, uint8 shift)
{
	return (uint8)((color >> shift) & 0xFF);
}

static GYcolor mixColor(GYcolor from, GYcolor to, int32 progress)
{
	if (progress < 0)
		progress = 0;
	if (progress > GY_ANIM_ONE)
		progress = GY_ANIM_ONE;
	return ((GYcolor)lerpI32(colorPart(from, 24), colorPart(to, 24), progress) << 24) |
		   ((GYcolor)lerpI32(colorPart(from, 16), colorPart(to, 16), progress) << 16) |
		   ((GYcolor)lerpI32(colorPart(from, 8), colorPart(to, 8), progress) << 8) |
		   (GYcolor)lerpI32(colorPart(from, 0), colorPart(to, 0), progress);
}

static void applyNode(GYanim_node* node, int32 progress)
{
	GYrect old_area;
	switch (node->kind)
	{
	case ANIM_MOVE:
		YMGUI_Obj_GetAbsArea(node->owner, &old_area);
		node->owner->area.x = lerpCoord(node->data.pair.x0, node->data.pair.x1, progress);
		node->owner->area.y = lerpCoord(node->data.pair.y0, node->data.pair.y1, progress);
		YMGUI_Ctx_InvalidateArea(node->owner->ctx, &old_area);
		YMGUI_Obj_Invalidate(node->owner);
		break;
	case ANIM_RESIZE:
		YMGUI_Obj_GetAbsArea(node->owner, &old_area);
		node->owner->area.w = lerpCoord(node->data.pair.x0, node->data.pair.x1, progress);
		node->owner->area.h = lerpCoord(node->data.pair.y0, node->data.pair.y1, progress);
		if (node->owner->area.w < 1)
			node->owner->area.w = 1;
		if (node->owner->area.h < 1)
			node->owner->area.h = 1;
		YMGUI_Ctx_InvalidateArea(node->owner->ctx, &old_area);
		YMGUI_Obj_Invalidate(node->owner);
		break;
	case ANIM_COLOR:
		node->owner->bg_color = mixColor(node->data.color.from, node->data.color.to, progress);
		YMGUI_Obj_Invalidate(node->owner);
		break;
	case ANIM_SCROLL:
		node->owner->scroll_x = lerpCoord(node->data.pair.x0, node->data.pair.x1, progress);
		node->owner->scroll_y = lerpCoord(node->data.pair.y0, node->data.pair.y1, progress);
		YMGUI_Obj_Invalidate(node->owner);
		break;
	case ANIM_VALUE:
		node->data.value.callback(lerpI32(node->data.value.from, node->data.value.to, progress), node->data.value.user);
		break;
	}
}

void YMGUI_Anim_Tick(GYCTX ctx, uint32 elapsed_ms)
{
	GYanim_manager* manager = getManager(ctx, 0);
	if (manager == NULL || elapsed_ms == 0 || manager->ticking)
		return;
	manager->ticking = 1;
	for (GYanim_node* node = manager->head; node != NULL; node = node->next)
	{
		if (node->dead)
			continue;
		uint64_t total = (uint64_t)node->elapsed_ms + elapsed_ms;
		node->elapsed_ms = total > UINT32_MAX ? UINT32_MAX : (uint32)total;
		if (node->elapsed_ms < node->delay_ms)
			continue;
		uint32 active = node->elapsed_ms - node->delay_ms;
		int32 raw = active >= node->duration_ms ? GY_ANIM_ONE : (int32)(((uint64_t)active * GY_ANIM_ONE) / node->duration_ms);
		int32 eased = manager->rate_cb != NULL ? manager->rate_cb(node->ease, raw, manager->rate_user) : defaultRate(node->ease, raw);
		applyNode(node, eased);
		if (active >= node->duration_ms)
			node->dead = 1;
	}
	manager->ticking = 0;
	sweep(manager);
}

void YMGUI_Anim_SetRateCallback(GYCTX ctx, GYanim_rate_cb callback, void* user_data)
{
	GYanim_manager* manager = getManager(ctx, callback != NULL);
	if (manager == NULL)
		return;
	manager->rate_cb = callback;
	manager->rate_user = user_data;
}

#endif // YMGUI_ANIM
