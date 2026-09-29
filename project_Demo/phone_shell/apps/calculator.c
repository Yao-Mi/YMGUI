#include "phone_ui.h"
#include "phone_host.h"

typedef struct
{
	GYOBJ calculator_display;
	int64_t calc_value, calc_accumulator;
	char calc_operator;
	int calc_fresh;
} AppState;
static AppState state;

static void calculator_key(GYOBJ obj);


static void calculator_key(GYOBJ obj)
{
	char key = ((PhoneButton*)obj->user_data)->value[0];
	if (key == 'C')
	{
		state.calc_value = state.calc_accumulator = state.calc_operator = 0;
		state.calc_fresh = 1;
	}
	else if (key >= '0' && key <= '9')
	{
		if (state.calc_fresh)
			state.calc_value = 0;
		state.calc_fresh = 0;
		if (state.calc_value <= 99999999)
			state.calc_value = state.calc_value * 10 + key - '0';
	}
	else
	{
		if (state.calc_operator && !state.calc_fresh)
		{
			if (state.calc_operator == '/' && state.calc_value == 0)
			{
				PhoneUI_text_set(state.calculator_display, "不能除以零");
				state.calc_value = state.calc_accumulator = state.calc_operator = 0;
				state.calc_fresh = 1;
				return;
			}
			if (state.calc_operator == '+')
				state.calc_value = state.calc_accumulator + state.calc_value;
			else if (state.calc_operator == '-')
				state.calc_value = state.calc_accumulator - state.calc_value;
			else if (state.calc_operator == '*')
				state.calc_value = state.calc_accumulator * state.calc_value;
			else if (state.calc_operator == '/')
				state.calc_value = state.calc_accumulator / state.calc_value;
			if (state.calc_value > 999999999 || state.calc_value < -999999999)
			{
				PhoneUI_text_set(state.calculator_display, "超出计算范围");
				state.calc_value = state.calc_accumulator = state.calc_operator = 0;
				state.calc_fresh = 1;
				return;
			}
		}
		state.calc_accumulator = state.calc_value;
		state.calc_operator = key == '=' ? 0 : key;
		state.calc_fresh = 1;
	}
	char value[40];
	snprintf(value, sizeof(value), "%lld", (long long)state.calc_value);
	PhoneUI_text_set(state.calculator_display, value);
}
static void app_create(GYOBJ view)
{
	state = (AppState){.calc_fresh = 1};
	PhoneUI_app_header(view, "计算器", "简简单单，算得明白");
	GYOBJ display = PhoneUI_panel(view, 18, 67, 284, 70, WHITE);
	state.calculator_display = PhoneUI_label_ex(display, 8, 17, 268, "0", INK, 3);
	static const char* keys[] = {"7", "8", "9", "+", "4", "5", "6", "-", "1", "2", "3", "*", "C", "0", "=", "/"};
	for (int i = 0; i < 16; ++i)
		PhoneUI_button(view, 18 + (i % 4) * 73, 158 + (i / 4) * 60, 65, 52, keys[i], calculator_key,
					   i % 4 == 3 || i == 14 ? ACCENT : RGB(129, 139, 160));
}

static void app_destroy(void)
{
	memset(&state, 0, sizeof(state));
}
static intptr_t app_inspect(const char* name, int index)
{
	(void)index;
	if (!strcmp(name, "calculator_display"))
		return (intptr_t)state.calculator_display;
	if (!strcmp(name, "calc_value"))
		return (intptr_t)state.calc_value;
	if (!strcmp(name, "calc_accumulator"))
		return (intptr_t)state.calc_accumulator;
	if (!strcmp(name, "calc_operator"))
		return (intptr_t)state.calc_operator;
	if (!strcmp(name, "calc_fresh"))
		return (intptr_t)state.calc_fresh;
	return 0;
}

const PhoneApp PhoneApp_calculator = {
	.key = "calculator", .title = "计算器", .color = RGB(124, 102, 196), .icon = ICON_CALCULATOR, .create = app_create, .inspect = app_inspect, .destroy = app_destroy};
