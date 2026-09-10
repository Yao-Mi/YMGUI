#include "studio.h"
#include "YMGUI_Button.h"
#include "YMGUI_Label.h"
#include <stdio.h>
#include <string.h>
static void cancel_apply(GYOBJ button, const GYval* value)
{
	YMGUI_Obj_SetHidden(button, !value->u.i);
}
static void export_apply(GYOBJ button, const GYval* value)
{
	YMGUI_Button_SetText(button, value->u.i ? "导出中…" : "导出 MP4");
}
void studio_export_bind(Studio* s)
{
	YMGUI_Bind_Attach(s->export_button, &s->export_running, export_apply);
	YMGUI_Bind_Attach(s->export_cancel, &s->export_running, cancel_apply);
	YMGUI_Label_Bind(s->export_label, &s->export_text);
	YMGUI_Bar_Bind(s->export_bar, &s->export_progress);
}
int studio_export_start(Studio* s, const char* path, int overwrite)
{
	StExportStatus status;
	st_export_status(s->exporter, &status);
	if (status.state == ST_EXPORT_RUNNING)
	{
		studio_status(s, "已有导出任务正在执行");
		return 0;
	}
	if (s->project_path[0] && !strcmp(s->project_path, path))
	{
		studio_status(s, "不能覆盖当前项目文件");
		return 0;
	}
	s->export_cancelling = 0;
	if (!st_export_start(s->exporter, &s->project, path, overwrite))
	{
		st_export_status(s->exporter, &status);
		studio_status(s, status.message);
		studio_export_poll(s);
		return 0;
	}
	studio_status(s, "正在后台导出 720p 无音频 MP4 · 可以继续剪辑");
	studio_export_poll(s);
	return 1;
}
void studio_export_poll(Studio* s)
{
	StExportStatus job;
	st_export_status(s->exporter, &job);
	int running = job.state == ST_EXPORT_RUNNING;
	int progress = job.total ? (int)((int64_t)job.frames * 1000 / job.total) : 0;
	if (running && progress >= 1000)
		progress = 990;
	if (job.state == ST_EXPORT_DONE)
		progress = 1000;
	YMGUI_State_SetBool(&s->export_running, running);
	YMGUI_State_SetInt(&s->export_progress, progress);
	char text[64];
	if (running)
		snprintf(text, sizeof(text), s->export_cancelling ? "正在取消…" : "导出 %d%%", progress / 10);
	else
		snprintf(text, sizeof(text), "%s", job.state == ST_EXPORT_IDLE ? "720p · 无音频" : job.state == ST_EXPORT_DONE	  ? "导出完成"
																					   : job.state == ST_EXPORT_CANCELLED ? "导出已取消"
																														  : "导出失败");
	if (strcmp(text, s->export_buffer))
	{
		strcpy(s->export_buffer, text);
		YMGUI_State_Touch(&s->export_text);
	}
	if (job.state != s->export_last_state && job.state >= ST_EXPORT_DONE)
	{
		char message[256];
		if (job.state == ST_EXPORT_DONE)
			snprintf(message, sizeof(message), "导出完成：%.200s", st_basename(job.path));
		else
			snprintf(message, sizeof(message), "%s", job.message);
		studio_status(s, message);
		s->export_cancelling = 0;
	}
	s->export_last_state = job.state;
}
