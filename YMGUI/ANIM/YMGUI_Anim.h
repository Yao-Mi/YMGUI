#ifndef YMGUI_ANIM_H
#define YMGUI_ANIM_H

#include "YMGUI_Obj.h"

#if YMGUI_ANIM

#ifdef __cplusplus
extern "C"
{
#endif

	typedef uint32 GYANIM;

	typedef enum
	{
		GY_ANIM_LINEAR = 0,
		GY_ANIM_SMOOTH,
		GY_ANIM_EASE_IN,
		GY_ANIM_EASE_OUT,
		GY_ANIM_OVERSHOOT,
		GY_ANIM_THERE_AND_BACK,
		GY_ANIM_EASE_COUNT
	} GYAnimEase;

	typedef void (*GYanim_value_cb)(int32 value, void* user_data);
	typedef void (*GYanim_done_cb)(GYANIM animation, uint8 cancelled, void* user_data);
	// 输入/输出均为 16.16：progress 位于 [0,65536]，返回值允许超调或回到 0。
	typedef int32 (*GYanim_rate_cb)(GYAnimEase ease, int32 progress, void* user_data);

	// 同一对象的同类内建属性动画会从当前状态平滑替换旧动画；ValueTo 是独立通道。
	// 返回 0 表示参数或内存失败。
	GYANIM YMGUI_Anim_MoveTo(GYOBJ obj, GYcoord x, GYcoord y, uint32 duration_ms, GYAnimEase ease);
	GYANIM YMGUI_Anim_ResizeTo(GYOBJ obj, GYcoord w, GYcoord h, uint32 duration_ms, GYAnimEase ease);
	GYANIM YMGUI_Anim_BgColorTo(GYOBJ obj, GYcolor color, uint32 duration_ms, GYAnimEase ease);
	GYANIM YMGUI_Anim_ScrollTo(GYOBJ obj, GYcoord x, GYcoord y, uint32 duration_ms, GYAnimEase ease);
	GYANIM YMGUI_Anim_ValueTo(GYCTX ctx, GYOBJ owner, int32 from, int32 to,
							  uint32 duration_ms, GYAnimEase ease, GYanim_value_cb callback, void* user_data);

	uint8 YMGUI_Anim_SetDelay(GYCTX ctx, GYANIM animation, uint32 delay_ms);
	uint8 YMGUI_Anim_SetDone(GYCTX ctx, GYANIM animation, GYanim_done_cb callback, void* user_data);
	uint8 YMGUI_Anim_Cancel(GYCTX ctx, GYANIM animation);
	void YMGUI_Anim_CancelObject(GYOBJ obj);
	void YMGUI_Anim_CancelAll(GYCTX ctx);
	uint16 YMGUI_Anim_ActiveCount(GYCTX ctx);

	// 通常由 YMGUI_Inject_Tick 自动调用；测试或无 HAL 主循环也可直接调用。
	void YMGUI_Anim_Tick(GYCTX ctx, uint32 elapsed_ms);
	// 可选外部缓动后端。传 NULL 恢复内建定点曲线；YMANIM 可通过此入口提供曲线采样。
	void YMGUI_Anim_SetRateCallback(GYCTX ctx, GYanim_rate_cb callback, void* user_data);

#ifdef __cplusplus
}
#endif
#endif // YMGUI_ANIM
#endif
