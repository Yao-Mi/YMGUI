#include "app/studio.h"
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static MsApp* dialog_app;
static void* fs_open(void* user, const char* path)
{
	(void)user;
	return opendir(path);
}
static int fs_read(void* user, void* dir, GYfiledialog_entry* e)
{
	(void)user;
	errno = 0;
	struct dirent* d = readdir(dir);
	if (!d)
		return errno ? -1 : 0;
	snprintf(e->name, e->name_cap, "%s", d->d_name);
	struct stat st;
	if (fstatat(dirfd(dir), d->d_name, &st, 0))
	{
		e->is_dir = 0;
		e->size = 0;
	}
	else
	{
		e->is_dir = S_ISDIR(st.st_mode);
		e->size = st.st_size > UINT32_MAX ? UINT32_MAX : (uint32)st.st_size;
	}
	return 1;
}
static void fs_close(void* user, void* d)
{
	(void)user;
	closedir(d);
}
static uint8 fs_stat(void* user, const char* path, uint8* exists, uint8* isdir)
{
	(void)user;
	struct stat st;
	if (stat(path, &st))
	{
		*exists = *isdir = 0;
		return errno == ENOENT;
	}
	*exists = 1;
	*isdir = S_ISDIR(st.st_mode);
	return 1;
}
static uint8 fs_mkdir(void* user, const char* path)
{
	(void)user;
	return mkdir(path, 0755) == 0;
}
static const char* translate(const char* key, void* user)
{
	(void)user;
	static const char* table[][2] = {
		{"Up", "上一级"}, {"Go", "进入"}, {"Refresh", "刷新"}, {"New dir", "新建目录"}, {"Open", "打开"}, {"Save", "保存"}, {"Select", "选择"}, {"Cancel", "取消"}, {"Path is too long", "路径过长"}, {"Cannot open directory", "无法打开目录"}, {"Directory read failed", "读取目录失败"}, {"Entry limit reached", "目录条目达到上限"}, {"Filesystem callbacks not set", "文件系统未连接"}, {"Cannot add tree node", "无法添加目录条目"}, {"Path loaded", "路径已载入"}, {"Choose a valid file name", "请选择有效文件名"}, {"Cannot inspect target", "无法读取目标信息"}, {"Select an existing file", "请选择已存在的文件"}, {"Target is a directory", "目标是目录"}, {"Overwrite not confirmed", "等待确认覆盖"}, {"Enter a valid folder name", "请输入有效目录名"}, {"Cannot create folder", "无法创建目录"}, {"Refreshed", "已刷新"}};
	for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); ++i)
		if (!strcmp(key, table[i][0]))
			return table[i][1];
	return key;
}
static int worker(void* user)
{
	MsApp* a = user;
	if (a->job_action == A_IMPORT || a->job_action == A_SAMPLE)
		a->job_ok = ms_asset_import(&a->job_asset, a->job_path, a->job_error, sizeof(a->job_error));
	else
		a->job_ok = ms_export(&a->project, &a->sounds, a->job_path, a->job_action == A_EXPORT_LOOP, a->job_error, sizeof(a->job_error));
	SDL_AtomicSet(&a->job_done, 1);
	return 0;
}
void ms_job_start(MsApp* a, int action, const char* path)
{
	if (a->worker)
		return;
	struct stat target, project_file;
	int same_file = a->path[0] && (!strcmp(path, a->path) ||
								   (!stat(path, &target) && !stat(a->path, &project_file) && target.st_dev == project_file.st_dev && target.st_ino == project_file.st_ino));
	if (same_file && (action == A_EXPORT || action == A_EXPORT_LOOP))
	{
		ms_status(a, "导出文件不能覆盖当前工程，请选择 WAV 文件名");
		return;
	}
	if ((action == A_IMPORT || action == A_SAMPLE) && a->project.assets == MS_ASSETS)
	{
		ms_status(a, "素材数量达到上限，请新建工程后导入");
		return;
	}
	a->transport.playing = 0;
	ms_transport_seek(&a->transport, ms_transport_position(&a->transport));
	snprintf(a->job_path, sizeof(a->job_path), "%s", path);
	a->job_action = action;
	a->job_error[0] = 0;
	a->job_ok = 0;
	SDL_AtomicSet(&a->job_done, 0);
	a->worker = SDL_CreateThread(worker, "music-file", a);
	ms_status(a, a->worker ? (action == A_IMPORT || action == A_SAMPLE ? "正在导入音频并生成波形……" : "正在导出立体声 WAV……") : "无法创建后台任务");
	ms_ui_refresh(a);
}
void ms_job_poll(MsApp* a)
{
	if (!a->worker || !SDL_AtomicGet(&a->job_done))
		return;
	SDL_WaitThread(a->worker, NULL);
	a->worker = NULL;
	if (!a->job_ok)
	{
		ms_status(a, a->job_error);
		return;
	}
	if (a->job_action == A_IMPORT || a->job_action == A_SAMPLE)
	{
		if (a->job_action == A_SAMPLE && a->job_asset.frames > MS_RATE * 5)
		{
			free(a->job_asset.pcm);
			memset(&a->job_asset, 0, sizeof(a->job_asset));
			ms_status(a, "鼓声采样最长 5 秒，请使用短打击乐采样");
			return;
		}
		int total = a->job_asset.frames;
		for (int i = 0; i < a->project.assets; ++i)
			total += a->project.asset[i].frames;
		if (total > MS_TOTAL_FRAMES)
		{
			free(a->job_asset.pcm);
			memset(&a->job_asset, 0, sizeof(a->job_asset));
			ms_status(a, "素材总时长超过 10 分钟限制");
			return;
		}
		ms_checkpoint(a);
		int index = a->project.assets++;
		a->project.asset[index] = a->job_asset;
		memset(&a->job_asset, 0, sizeof(a->job_asset));
		a->asset = index;
		if (a->job_action == A_SAMPLE)
			a->project.song.drum_asset[a->drum] = index;
		else
			a->library_tab = 1;
		ms_changed(a);
		ms_status(a, a->job_action == A_SAMPLE ? "鼓声已替换，点击鼓名试听" : "音频已导入；从左侧拖入音频轨，或点击加入编排");
	}
	else
		ms_status(a, "导出完成：48 千赫兹、16 位、立体声 WAV");
}
int ms_save(MsApp* a, const char* path)
{
	char error[256];
	if (!ms_project_save(&a->project, path, error, sizeof(error)))
	{
		ms_status(a, error);
		return 0;
	}
	if (path != a->path)
		snprintf(a->path, sizeof(a->path), "%s", path);
	a->dirty = 0;
	ms_ui_refresh(a);
	ms_status(a, "工程已保存，包含全部音频素材");
	return 1;
}
int ms_load(MsApp* a, const char* path)
{
	char error[256];
	int64_t position = ms_transport_position(&a->transport);
	a->transport.playing = 0;
	ms_transport_seek(&a->transport, position);
	if (!ms_project_load(&a->project, path, error, sizeof(error)))
	{
		ms_ui_refresh(a);
		ms_status(a, error);
		return 0;
	}
	snprintf(a->path, sizeof(a->path), "%s", path);
	memset(a->history, 0, sizeof(*a->history));
	a->transport.playing = 0;
	ms_transport_seek(&a->transport, 0);
	a->dirty = 0;
	a->clip = -1;
	a->pattern = a->project.song.patterns ? 0 : -1;
	ms_notes_clear_selection(a);
	a->clipboard_valid = 0;
	a->step = -1;
	a->track = a->project.song.tracks ? 0 : -1;
	a->asset = a->project.assets ? 0 : -1;
	a->scroll = 0;
	a->transport.pattern = a->pattern;
	ms_ui_refresh(a);
	ms_status(a, "工程已打开");
	return 1;
}
static void run_path(MsApp* a, int action, const char* path)
{
	if (action == A_SAVE || action == A_SAVE_AS)
		ms_save(a, path);
	else if (action == A_OPEN)
		ms_load(a, path);
	else
		ms_job_start(a, action, path);
}
static void confirm_result(GYOBJ box, int index)
{
	(void)box;
	MsApp* a = dialog_app;
	if (index != 1)
		return;
	if (a->confirm_action == -1)
		a->quit = 1;
	else if (a->confirm_action == -2)
	{
		run_path(a, a->overwrite_action, a->pending);
		YMGUI_FileDialog_Close(a->dialog);
	}
	else
	{
		int dirty = a->dirty;
		a->dirty = 0;
		ms_action(a, (MsAction)a->confirm_action);
		if (a->confirm_action == A_OPEN)
			a->dirty = dirty;
	}
}
void ms_confirm(MsApp* a, int action, const char* text)
{
	dialog_app = a;
	a->confirm_action = action;
	YMGUI_MsgBox_ClearButtons(a->msgbox);
	YMGUI_MsgBox_SetTitle(a->msgbox, "确认操作");
	YMGUI_MsgBox_SetText(a->msgbox, text);
	YMGUI_MsgBox_AddButton(a->msgbox, "取消", confirm_result);
	YMGUI_MsgBox_AddButton(a->msgbox, action == -2 ? "覆盖保存" : "继续", confirm_result);
	YMGUI_MsgBox_Show(a->msgbox);
}
static uint8 overwrite(GYOBJ dialog, const char* path, void* user)
{
	(void)dialog;
	MsApp* a = user;
	snprintf(a->pending, sizeof(a->pending), "%s", path);
	a->overwrite_action = a->dialog_action;
	ms_confirm(a, -2, "目标文件已存在，成功写入后才替换原文件。是否覆盖？");
	return 0;
}
static void result(GYOBJ dialog, uint8 accepted, const char* path, void* user)
{
	(void)dialog;
	MsApp* a = user;
	if (!accepted)
		return;
	snprintf(a->directory, sizeof(a->directory), "%s", path);
	char* slash = strrchr(a->directory, '/');
	if (slash)
	{
		if (slash == a->directory)
			slash[1] = 0;
		else
			*slash = 0;
	}
	run_path(a, a->dialog_action, path);
}
void ms_dialog(MsApp* a, MsAction action)
{
	dialog_app = a;
	a->dialog_action = action;
	GYfiledialog_fs fs = {fs_open, fs_read, fs_close, fs_stat, fs_mkdir};
	YMGUI_FileDialog_SetFS(a->dialog, &fs, NULL);
	YMGUI_FileDialog_SetResultCb(a->dialog, result, a);
	YMGUI_FileDialog_SetOverwriteCb(a->dialog, overwrite);
	YMGUI_FileDialog_SetTranslator(a->dialog, translate, NULL);
	int saving = action == A_SAVE || action == A_SAVE_AS || action == A_EXPORT || action == A_EXPORT_LOOP;
	if (!a->directory[0] && !getcwd(a->directory, sizeof(a->directory)))
		strcpy(a->directory, "/");
	const char* name = action == A_EXPORT || action == A_EXPORT_LOOP ? "我的作品.wav" : "我的作品.ymmusic";
	if (!YMGUI_FileDialog_Show(a->dialog, saving ? GY_FILE_DIALOG_SAVE_FILE : GY_FILE_DIALOG_OPEN_FILE, a->directory, saving ? name : ""))
		ms_status(a, "无法打开目录，请检查路径权限");
}
int ms_close_request(void* user)
{
	MsApp* a = user;
	if (YMGUI_MsgBox_IsShown(a->msgbox))
	{
		YMGUI_MsgBox_Close(a->msgbox);
		return 0;
	}
	if (YMGUI_FileDialog_IsShown(a->dialog))
	{
		YMGUI_FileDialog_Close(a->dialog);
		return 0;
	}
	if (a->worker)
	{
		ms_status(a, "请等待当前文件任务结束后退出");
		return 0;
	}
	if (!a->dirty)
		return 1;
	ms_confirm(a, -1, "作品尚未保存，退出将丢失当前修改。是否继续？");
	return 0;
}
