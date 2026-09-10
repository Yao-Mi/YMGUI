#include "app/studio.h"
#include "YMGUI_FileDialog.h"
#include "YMGUI_MsgBox.h"
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static void* fs_open(void* u, const char* path)
{
	(void)u;
	return opendir(path);
}
static int fs_read(void* u, void* dir, GYfiledialog_entry* e)
{
	(void)u;
	struct dirent* d = readdir(dir);
	if (!d)
		return 0;
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
static void fs_close(void* u, void* d)
{
	(void)u;
	closedir(d);
}
static uint8 fs_stat(void* u, const char* path, uint8* exists, uint8* is_dir)
{
	(void)u;
	struct stat st;
	if (stat(path, &st))
	{
		*exists = 0;
		*is_dir = 0;
		return errno == ENOENT;
	}
	*exists = 1;
	*is_dir = S_ISDIR(st.st_mode);
	return 1;
}
static uint8 fs_mkdir(void* u, const char* path)
{
	(void)u;
	return mkdir(path, 0755) == 0;
}
static int save(Studio* s, const char* path)
{
	if (st_project_save(&s->project, path))
	{
		snprintf(s->project_path, sizeof(s->project_path), "%s", path);
		s->dirty = 0;
		studio_ui_refresh(s);
		studio_status(s, "项目已保存");
		return 1;
	}
	studio_status(s, "保存失败，请检查目标路径与写入权限");
	return 0;
}
static void confirm_result(GYOBJ box, int index)
{
	Studio* s = studio_of(box);
	if (index != 1)
		return;
	if (s->modal_action == -2)
		s->quit = 1;
	else if (s->modal_action == -3)
	{
		if (s->dialog_action == ACT_EXPORT ? studio_export_start(s, s->pending_path, 1) : save(s, s->pending_path))
			YMGUI_FileDialog_Close(s->dialog);
	}
	else if (s->modal_action == ACT_OPEN)
	{
		s->dirty = 0;
		studio_dialog_show(s, ACT_OPEN);
		s->dirty = 1;
	}
}
void studio_confirm(Studio* s, int action, const char* text)
{
	s->modal_action = action;
	YMGUI_MsgBox_ClearButtons(s->msgbox);
	YMGUI_MsgBox_SetTitle(s->msgbox, "确认操作");
	YMGUI_MsgBox_SetText(s->msgbox, text);
	YMGUI_MsgBox_AddButton(s->msgbox, "取消", confirm_result);
	YMGUI_MsgBox_AddButton(s->msgbox, action == -3 ? "覆盖保存" : "继续", confirm_result);
	YMGUI_MsgBox_Show(s->msgbox);
}
static uint8 overwrite(GYOBJ dialog, const char* path, void* user)
{
	(void)dialog;
	Studio* s = user;
	snprintf(s->pending_path, sizeof(s->pending_path), "%s", path);
	studio_confirm(s, -3, s->dialog_action == ACT_EXPORT ? "目标 MP4 已存在。导出成功后才会替换原文件。" : "目标项目文件已存在，是否覆盖保存？");
	return 0;
}
static void result(GYOBJ dialog, uint8 accepted, const char* path, void* user)
{
	(void)dialog;
	Studio* s = user;
	if (!accepted)
		return;
	if (s->dialog_action == ACT_IMPORT)
		studio_import(s, path);
	else if (s->dialog_action == ACT_EXPORT)
		studio_export_start(s, path, 0);
	else if (s->dialog_action == ACT_SAVE)
		save(s, path);
	else
	{
		StProject* p = malloc(sizeof(*p));
		if (!p)
			return;
		if (!st_project_load(p, path))
		{
			studio_status(s, "打开失败：项目格式无效或版本不支持");
			free(p);
			return;
		}
		s->project = *p;
		s->thumb_count = 0;
		memset(s->proxy_states, 0, sizeof(s->proxy_states));
		free(p);
		s->dirty = 0;
		snprintf(s->project_path, sizeof(s->project_path), "%s", path);
		st_history_reset(&s->history, &s->project);
		s->selected_clip = -1;
		s->selected_media = s->project.media_count ? 0 : -1;
		s->selected_track = 0;
		studio_mode(s, 0);
		studio_changed(s);
		for (int i = 0; i < s->project.media_count; i++)
		{
			if (st_engine_import(s->engine, s->project.media[i].path))
				s->pending_imports++;
		}
		studio_status(s, "项目已打开，正在读取素材缩略图");
	}
}
void studio_dialog_init(Studio* s)
{
	s->dialog = YMGUI_Creat_FileDialog_Creat(s->ctx, 900, 600, 1023, 255, 512);
	GYfiledialog_fs fs = {fs_open, fs_read, fs_close, fs_stat, fs_mkdir};
	YMGUI_FileDialog_SetFS(s->dialog, &fs, NULL);
	YMGUI_FileDialog_SetResultCb(s->dialog, result, s);
	YMGUI_FileDialog_SetOverwriteCb(s->dialog, overwrite);
	s->msgbox = YMGUI_Creat_MsgBox_Creat(s->ctx);
}
void studio_dialog_show(Studio* s, int action)
{
	if (action == ACT_EXPORT)
	{
		StExportStatus job;
		st_export_status(s->exporter, &job);
		if (job.state == ST_EXPORT_RUNNING)
		{
			studio_status(s, "已有导出任务正在执行");
			return;
		}
		if (!st_duration(&s->project))
		{
			studio_status(s, "请先把素材加入时间线再导出");
			return;
		}
		studio_status(s, "导出为 H.264 MP4 · 1280x720 · 25 fps · 无音频");
	}
	if (action == ACT_OPEN && s->pending_imports)
	{
		studio_status(s, "请等待当前素材导入完成，再打开项目");
		return;
	}
	if (action == ACT_OPEN && s->dirty)
	{
		studio_confirm(s, ACT_OPEN, "项目尚未保存。打开其他项目将丢弃当前修改。");
		return;
	}
	YMGUI_State_SetBool(&s->playing, 0);
	s->dialog_action = action;
	char cwd[1024];
	if (!getcwd(cwd, sizeof(cwd)))
		strcpy(cwd, "/");
	char name[1024] = "未命名.ymstudio";
	if (action == ACT_SAVE && s->project_path[0])
	{
		strcpy(cwd, s->project_path);
		char* slash = strrchr(cwd, '/');
		if (slash)
		{
			strcpy(name, slash + 1);
			if (slash == cwd)
				slash[1] = 0;
			else
				*slash = 0;
		}
	}
	if (action == ACT_EXPORT)
		snprintf(name, sizeof(name), "%s", "成片.mp4");
	int saving = action == ACT_SAVE || action == ACT_EXPORT;
	YMGUI_FileDialog_Show(s->dialog, saving ? GY_FILE_DIALOG_SAVE_FILE : GY_FILE_DIALOG_OPEN_FILE, cwd, saving ? name : "");
}
