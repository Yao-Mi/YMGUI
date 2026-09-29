#include "phone_ui.h"
#include "phone_host.h"

typedef struct
{
	GYOBJ file_path, file_rows[4], file_preview;
	int file_folder;
} AppState;
static AppState state;

static int app_back(void);
static void file_refresh(void);
static void file_open(GYOBJ obj);


static const char* folder_names[] = {"照片", "录音", "笔记", "下载"};
static int app_back(void)
{
	if (state.file_folder < 0)
		return 0;
	state.file_folder = -1;
	file_refresh();
	return 1;
}

static void file_refresh(void)
{
	char path[64];
	snprintf(path, sizeof(path), "演示存储 / %s", state.file_folder < 0 ? "" : folder_names[state.file_folder]);
	PhoneUI_text_set(state.file_path, path);
	static const char* entries[4][4] = {
		{"山间日落（示例）", "海边晨光（示例）", "夜色（示例）", "返回上一级"},
		{"查看演示录音记录", "未生成音频文件", "未接入麦克风", "返回上一级"},
		{"打开当前笔记", "仅在本次运行保存", "未写入磁盘", "返回上一级"},
		{"暂无下载内容", "浏览器尚未联网", "演示文件系统", "返回上一级"}};
	for (int i = 0; i < 4; ++i)
		PhoneUI_button_set(state.file_rows[i], state.file_folder < 0 ? folder_names[i] : entries[state.file_folder][i]);
	YMGUI_TextView_SetText(state.file_preview, "此处仅展示虚拟目录，不访问真实磁盘。");
}

static void file_open(GYOBJ obj)
{
	int index = 0;
	for (int i = 0; i < 4; ++i)
		if (obj == state.file_rows[i])
			index = i;
	if (state.file_folder < 0)
	{
		state.file_folder = index;
		file_refresh();
		return;
	}
	if (index == 3)
	{
		state.file_folder = -1;
		file_refresh();
		return;
	}
	if (state.file_folder == 2 && index == 0)
	{
		PhoneHost_Open(NOTES, NULL);
		return;
	}
	if (state.file_folder == 0)
	{
		PhoneHost_Open(GALLERY, NULL);
		return;
	}
	char text[128];
	snprintf(text, sizeof(text), state.file_folder == 1 ? "本次运行有 %d 条演示记录。\n不包含声音数据。" : "暂无真实文件。\n仅演示目录与打开/返回流程。", PhoneApps_Query(RECORDER, "record-count"));
	YMGUI_TextView_SetText(state.file_preview, text);
}
static void app_create(GYOBJ view)
{
	state = (AppState){.file_folder = -1};
	PhoneUI_app_header(view, "文件管理器", "虚拟目录 · 不读写真实磁盘");
	state.file_path = PhoneUI_left_label(view, 24, 65, 272, "", MUTED, 2);
	for (int i = 0; i < 4; ++i)
		state.file_rows[i] = PhoneUI_button(view, 24, 99 + i * 49, 272, 41, "", file_open, RGB(160, 143, 105));
	state.file_preview = PhoneUI_app_text(view, 24, 307, 272, 92, "");
	file_refresh();
}

static void app_destroy(void)
{
	memset(&state, 0, sizeof(state));
}
static intptr_t app_inspect(const char* name, int index)
{
	(void)index;
	if (!strcmp(name, "file_path"))
		return (intptr_t)state.file_path;
	if (!strcmp(name, "file_rows"))
		return index >= 0 && index < 4 ? (intptr_t)state.file_rows[index] : 0;
	if (!strcmp(name, "file_preview"))
		return (intptr_t)state.file_preview;
	if (!strcmp(name, "file_folder"))
		return (intptr_t)state.file_folder;
	return 0;
}

const PhoneApp PhoneApp_files = {
	.key = "files", .title = "文件管理", .color = RGB(231, 173, 65), .icon = ICON_FILES, .create = app_create, .inspect = app_inspect, .back = app_back, .destroy = app_destroy};
