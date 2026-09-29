#include "phone_ui.h"
#include "phone_host.h"

typedef struct
{
	GYOBJ clock_label, clock_button;
	int stopwatch_ms, stopwatch_running;
} AppState;
static AppState state;

static void app_tick(uint32 elapsed);
static void app_close(void);
static void clock_toggle(GYOBJ obj);
static void clock_reset(GYOBJ obj);


static void app_tick(uint32 elapsed)
{
	if (state.stopwatch_running)
	{
		state.stopwatch_ms = (state.stopwatch_ms + elapsed) % 3600000;
		char time[24];
		snprintf(time, sizeof(time), "%02d:%02d.%d", state.stopwatch_ms / 60000, state.stopwatch_ms / 1000 % 60, state.stopwatch_ms / 100 % 10);
		PhoneUI_text_set(state.clock_label, time);
	}
}
static void app_close(void)
{
	state.stopwatch_running = 0;
	PhoneUI_button_set(state.clock_button, "开始");
}

static void clock_toggle(GYOBJ obj)
{
	state.stopwatch_running = !state.stopwatch_running;
	PhoneUI_button_set(obj, state.stopwatch_running ? "暂停" : "开始");
}

static void clock_reset(GYOBJ obj)
{
	(void)obj;
	state.stopwatch_ms = state.stopwatch_running = 0;
	PhoneUI_text_set(state.clock_label, "00:00.0");
	PhoneUI_button_set(state.clock_button, "开始");
}
static void app_create(GYOBJ view)
{
	state = (AppState){0};
	PhoneUI_app_header(view, "时钟", "珍惜每一刻");
	GYOBJ dial = PhoneUI_panel(view, 126, 89, 68, 68, PhoneApps_Get(CLOCK)->color);
	dial->draw_cb = PhoneUI_icon_tile_draw;
	dial->user_data = (void*)(intptr_t)ICON_CLOCK;
	state.clock_label = PhoneUI_label_ex(view, 12, 181, 296, "00:00.0", INK, 1);
	PhoneUI_label(view, 22, 257, 276, "秒表", MUTED);
	state.clock_button = PhoneUI_button(view, 18, 340, 136, 46, "开始", clock_toggle, ACCENT);
	PhoneUI_button(view, 166, 340, 136, 46, "重置", clock_reset, RGB(137, 146, 168));
}

static void app_destroy(void)
{
	memset(&state, 0, sizeof(state));
}
static intptr_t app_inspect(const char* name, int index)
{
	(void)index;
	if (!strcmp(name, "clock_label"))
		return (intptr_t)state.clock_label;
	if (!strcmp(name, "clock_button"))
		return (intptr_t)state.clock_button;
	if (!strcmp(name, "stopwatch_ms"))
		return (intptr_t)state.stopwatch_ms;
	if (!strcmp(name, "stopwatch_running"))
		return (intptr_t)state.stopwatch_running;
	return 0;
}

const PhoneApp PhoneApp_clock = {
	.key = "clock", .title = "时钟", .color = RGB(75, 89, 117), .icon = ICON_CLOCK, .create = app_create, .inspect = app_inspect, .tick = app_tick, .close = app_close, .destroy = app_destroy};
