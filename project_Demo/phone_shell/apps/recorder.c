#include "phone_ui.h"
#include "phone_host.h"

typedef struct
{
	GYOBJ record_time, record_button, record_status, record_meter;
	int recording, record_ms, recording_count, record_peak_ms;
} AppState;
static AppState state;

static void app_tick(uint32 elapsed);
static void app_close(void);
static int app_query(const char* name);
static void record_toggle(GYOBJ obj);


static void app_tick(uint32 elapsed)
{
	if (!state.recording)
		return;
	state.record_ms = (state.record_ms + elapsed) % 3600000;
	char text[96];
	snprintf(text, sizeof(text), "%02d:%02d", state.record_ms / 60000, state.record_ms / 1000 % 60);
	PhoneUI_text_set(state.record_time, text);
	state.record_peak_ms += elapsed;
	if (state.record_peak_ms >= 400)
	{
		state.record_peak_ms = 0;
		PhoneUI_animate_bar(state.record_meter, 20 + (state.record_ms / 400 * 37) % 75, 350);
	}
}
static void app_close(void)
{
	if (!state.recording)
		return;
	state.recording = 0;
	PhoneUI_button_set(state.record_button, "开始演示录音");
	PhoneUI_text_set(state.record_status, "演示已停止 · 未采集声音");
	YMGUI_Bar_SetValue(state.record_meter, 0);
}
static int app_query(const char* name)
{
	return !strcmp(name, "record-count") ? state.recording_count : 0;
}

static void record_toggle(GYOBJ obj)
{
	(void)obj;
	if (!state.recording)
		state.record_ms = state.record_peak_ms = 0;
	state.recording = !state.recording;
	PhoneUI_button_set(state.record_button, state.recording ? "停止演示" : "开始演示录音");
	if (state.recording)
		PhoneUI_text_set(state.record_status, "计时中 · 不采集麦克风声音");
	else
	{
		++state.recording_count;
		char text[128];
		snprintf(text, sizeof(text), "演示记录 %d · %d 秒 · 无音频文件", state.recording_count, state.record_ms / 1000);
		PhoneUI_text_set(state.record_status, text);
		PhoneUI_animate_bar(state.record_meter, 0, 200);
	}
}
static void app_create(GYOBJ view)
{
	state = (AppState){0};
	PhoneUI_app_header(view, "录音机", "状态演示 · 未接入麦克风");
	GYOBJ mark = PhoneUI_panel(view, 126, 76, 68, 68, PhoneApps_Get(RECORDER)->color);
	mark->draw_cb = PhoneUI_icon_tile_draw;
	mark->user_data = (void*)(intptr_t)ICON_RECORDER;
	state.record_time = PhoneUI_label_ex(view, 20, 166, 280, "00:00", INK, 1);
	state.record_meter = YMGUI_Creat_Bar_Creat(view, 28, 251, 264, 7);
	YMGUI_Bar_SetRange(state.record_meter, 0, 100);
	YMGUI_Bar_SetColors(state.record_meter, RGB(230, 219, 222), PhoneApps_Get(RECORDER)->color);
	state.record_status = PhoneUI_label_ex(view, 12, 286, 296, "点击开始体验录音状态", MUTED, 2);
	state.record_button = PhoneUI_button(view, 24, 351, 272, 43, "开始演示录音", record_toggle, PhoneApps_Get(RECORDER)->color);
}

static void app_destroy(void)
{
	memset(&state, 0, sizeof(state));
}
static intptr_t app_inspect(const char* name, int index)
{
	(void)index;
	if (!strcmp(name, "record_time"))
		return (intptr_t)state.record_time;
	if (!strcmp(name, "record_button"))
		return (intptr_t)state.record_button;
	if (!strcmp(name, "record_status"))
		return (intptr_t)state.record_status;
	if (!strcmp(name, "record_meter"))
		return (intptr_t)state.record_meter;
	if (!strcmp(name, "recording"))
		return (intptr_t)state.recording;
	if (!strcmp(name, "record_ms"))
		return (intptr_t)state.record_ms;
	if (!strcmp(name, "recording_count"))
		return (intptr_t)state.recording_count;
	if (!strcmp(name, "record_peak_ms"))
		return (intptr_t)state.record_peak_ms;
	return 0;
}

const PhoneApp PhoneApp_recorder = {
	.key = "recorder", .title = "录音机", .color = RGB(224, 94, 103), .icon = ICON_RECORDER, .create = app_create, .inspect = app_inspect, .tick = app_tick, .close = app_close, .query = app_query, .destroy = app_destroy};
