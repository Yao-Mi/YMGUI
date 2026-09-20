#include "app/studio.h"
#include "SDL_LCD.h"
#include "YMGUI_Mem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <math.h>

extern const uint16 YMGUI_GB2312_cps[];
extern const uint16 YMGUI_GB2312_glyph_count;
static FILE* blob;
static uint32 font_read(const GYfont* font, uint32 off, uint32 len, uint8* buf)
{
	(void)font;
	if (!blob || fseek(blob, (long)off, SEEK_SET))
		return 0;
	return (uint32)fread(buf, 1, len, blob);
}
static GYfont font = {NULL, YMGUI_GB2312_cps, 0, 0, 0, 16, 16, 8, 4, NULL, font_read};
static SDL_atomic_t shortcut;
static int key_watch(void* user, SDL_Event* event)
{
	(void)user;
	if (event->type == SDL_KEYDOWN && !event->key.repeat && event->key.windowID == SDL_LCD_WindowId() && (event->key.keysym.mod & KMOD_CTRL))
	{
		if (event->key.keysym.sym == SDLK_s)
			SDL_AtomicSet(&shortcut, A_SAVE + 1);
		if (event->key.keysym.sym == SDLK_y)
			SDL_AtomicSet(&shortcut, A_REDO + 1);
	}
	return 1;
}
static uint8 key_filter(uint32 key, uint8 pressed, void* user)
{
	MsApp* a = user;
	/* SDL 失焦也会注入修饰键抬起；即使正忙或拖动，也必须更新。 */
	if (key == GY_KEY_CTRL || key == GY_KEY_SHIFT)
	{
		if (key == GY_KEY_CTRL)
			a->ctrl = pressed != 0;
		else
			a->shift = pressed != 0;
		return 1;
	}
	if (!pressed || a->worker)
		return 0;
	if (a->drag == 2 || a->drag == 6 || a->drag == 7)
		return 1;
	if (YMGUI_FileDialog_IsShown(a->dialog) || YMGUI_MsgBox_IsShown(a->msgbox) || ms_instrument_popup_open(a))
		return 0;
	GYOBJ focus = a->ctx->focus_obj;
	if (focus == a->track_name || focus == a->pattern_name || focus == a->bpm_input || focus == a->fade_in || focus == a->fade_out)
		return 0;
	if (key == ' ')
	{
		ms_action(a, A_PLAY);
		return 1;
	}
	if (key == GY_KEY_ENTER)
	{
		ms_action(a, A_STOP);
		return 1;
	}
	if (key == GY_KEY_UNDO)
	{
		ms_action(a, A_UNDO);
		return 1;
	}
	if (key == GY_KEY_DEL)
	{
		ms_action(a, a->tab == 1 && (focus == a->editor || (a->select_notes && focus != a->timeline && focus != a->library)) ? A_NOTE_DELETE : A_DELETE);
		return 1;
	}
	if (key == GY_KEY_COPY)
	{
		if (a->clip >= 0 && a->clip < a->project.song.clips)
		{
			a->clipboard = a->project.song.clip[a->clip];
			a->clipboard_valid = 1;
			ms_status(a, "片段已复制；选择同类轨道和播放位置后按 Ctrl+V 粘贴");
		}
		return 1;
	}
	if (key == GY_KEY_PASTE)
	{
		MsSong* s = &a->project.song;
		if (a->clipboard_valid && a->track >= 0 && a->clipboard.track < s->tracks && s->track[a->track].kind == s->track[a->clipboard.track].kind)
		{
			MsClip c = a->clipboard;
			c.track = a->track;
			c.start = (int)ms_frame_tick(s, ms_transport_position(&a->transport)) / MS_STEP * MS_STEP;
			ms_checkpoint(a);
			int n = ms_clip_add(s, c.track, c.source, c.start, c.length);
			if (n >= 0)
			{
				s->clip[n] = c;
				a->clip = n;
				ms_changed(a);
			}
		}
		return 1;
	}
	return 0;
}
static void demo_audio(MsProject* p, const MsSounds* sounds)
{
	MsAsset* a = &p->asset[0];
	a->frames = (int)ms_tick_frame(&p->song, 1536);
	a->pcm = calloc((size_t)a->frames * 2, sizeof(float));
	if (!a->pcm)
	{
		a->frames = 0;
		return;
	}
	strcpy(a->name, "采样和弦铺底");
	p->assets = 1;
	for (int i = 0; i < a->frames; ++i)
	{
		float fade = fminf(i / 4800.f, (a->frames - i) / 9600.f);
		if (fade > 1)
			fade = 1;
		float v = (ms_instrument_sample(sounds, 130.8128, 5, i, a->frames) +
				   ms_instrument_sample(sounds, 164.8138, 5, i, a->frames) +
				   ms_instrument_sample(sounds, 195.9977, 5, i, a->frames)) *
				  .3f * fade;
		a->pcm[i * 2] = v;
		a->pcm[i * 2 + 1] = v;
	}
	ms_asset_peaks(a);
	ms_clip_add(&p->song, 2, 0, 0, 1536);
	p->song.track[2].volume = .35f;
}
int main(int argc, char** argv)
{
	MsApp* a = calloc(1, sizeof(*a));
	if (!a)
		return 1;
	int result = 1;
	const char* open_path = NULL;
	const char* export_path = NULL;
	int tab = 0;
	a->frames = -1;
	for (int i = 1; i < argc; ++i)
	{
		if (!strcmp(argv[i], "--frames") && i + 1 < argc)
			a->frames = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--project") && i + 1 < argc)
			open_path = argv[++i];
		else if (!strcmp(argv[i], "--export") && i + 1 < argc)
			export_path = argv[++i];
		else if (!strcmp(argv[i], "--tab") && i + 1 < argc)
			tab = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--selftest"))
			a->selftest = 1;
		else if (argv[i][0] >= '0' && argv[i][0] <= '9')
			a->frames = atoi(argv[i]);
		else
		{
			fprintf(stderr, "用法：music_studio [帧数] [--project 工程] [--export WAV] [--tab 0|1|2] [--selftest]\n");
			free(a);
			return 2;
		}
	}
	a->history = calloc(1, sizeof(*a->history));
	a->gesture = calloc(1, sizeof(*a->gesture));
	if (!a->history || !a->gesture)
		goto done;
	ms_project_init(&a->project, 1);
	if (!ms_sounds_init(&a->sounds))
		goto done;
	demo_audio(&a->project, &a->sounds);
	a->clip = -1;
	a->asset = 0;
	a->pattern = 0;
	a->note = -1;
	a->step = -1;
	a->note_velocity = 100;
	a->note_length = MS_STEP * 2;
	a->octave = 5;
	a->snap = 1;
	a->zoom = .66;
	a->editor_zoom = 1;
	if (open_path)
	{
		char error[256];
		if (!ms_project_load(&a->project, open_path, error, sizeof(error)))
		{
			fprintf(stderr, "%s\n", error);
			goto done;
		}
		snprintf(a->path, sizeof(a->path), "%s", open_path);
	}
	if (export_path)
	{
		char error[256];
		result = ms_export(&a->project, &a->sounds, export_path, 0, error, sizeof(error)) ? 0 : 1;
		if (result)
			fprintf(stderr, "%s\n", error);
		goto done;
	}
	GYdisp disp = {0};
	disp.hor_res = MS_W;
	disp.ver_res = MS_H;
	disp.buf_px_cnt = MS_W * 32;
	disp.buf1 = GY_malloc1(disp.buf_px_cnt * sizeof(GYpx));
	if (!disp.buf1)
		goto done;
	if (SDL_LCD_Init(&disp, 1))
	{
		GY_free1(disp.buf1);
		goto done;
	}
	SDL_LCD_SetTitle("音乐工坊 · YMGUI");
	a->ctx = YMGUI_Creat_Ctx_Creat(&disp, MS_W, MS_H);
	if (!a->ctx)
	{
		SDL_LCD_Destroy();
		GY_free1(disp.buf1);
		goto done;
	}
	blob = fopen(GB2312_BIN_PATH, "rb");
	if (blob)
	{
		font.glyph_count = YMGUI_GB2312_glyph_count;
		YMGUI_Font_SetFallback(&font);
	}
	else
	{
		fprintf(stderr, "无法打开中文字库：%s\n", GB2312_BIN_PATH);
		goto ui_done;
	}
	ms_transport_init(&a->transport);
	ms_ui_build(a);
	YMGUI_Inject_SetCtx(a->ctx);
	YMGUI_Inject_SetKeyFilter(key_filter, a);
	SDL_LCD_SetCloseRequestCb(ms_close_request, a);
	SDL_AddEventWatch(key_watch, a);
	ms_status(a, a->transport.device ? "欢迎使用音乐工坊：空格播放示例，点击鼓点制作节奏；修改名称后按回车确认" : "声卡未打开；可以编辑与导出 WAV，重新启动前检查音频设备");
	if (tab == 1)
		ms_action(a, A_PIANO);
	if (tab == 2)
		ms_action(a, A_MIXER);
	mkdir(MS_DATA_DIR, 0755);
	a->autosave_at = SDL_GetTicks();
	if (a->selftest)
	{
		result = ms_ui_selftest(a) ? 0 : 1;
		a->frames = 3;
	}
	else
		result = 0;
	int frame = 0;
	while (!a->quit && a->frames != 0 && SDL_LCD_PumpEvents())
	{
		int action = SDL_AtomicGet(&shortcut);
		if (action)
		{
			SDL_AtomicSet(&shortcut, 0);
			if (!YMGUI_FileDialog_IsShown(a->dialog) && !YMGUI_MsgBox_IsShown(a->msgbox) && !ms_instrument_popup_open(a))
				ms_action(a, (MsAction)(action - 1));
		}
		ms_job_poll(a);
		int playing = a->transport.playing;
		ms_transport_tick(&a->transport, &a->project, &a->sounds);
		if (playing != a->transport.playing)
			ms_ui_refresh(a);
		if (frame % 3 == 0)
		{
			double ticks = ms_frame_tick(&a->project.song, ms_transport_position(&a->transport));
			char b[96];
			int beat = (int)ticks / MS_PPQ;
			snprintf(b, sizeof(b), "第 %03d 小节  第 %d 拍  |  %02d:%02d.%02d", beat / a->project.song.beats + 1, beat % a->project.song.beats + 1,
					 (int)(ticks * 60 / a->project.song.bpm / MS_PPQ) / 60, (int)(ticks * 60 / a->project.song.bpm / MS_PPQ) % 60, (int)(ticks * 6000 / a->project.song.bpm / MS_PPQ) % 100);
			YMGUI_Label_SetText(a->clock_label, b);
			if (a->transport.playing)
			{
				YMGUI_Obj_Invalidate(a->timeline);
				YMGUI_Obj_Invalidate(a->editor);
			}
		}
		if (!a->selftest && !a->worker && a->dirty && !a->drag && SDL_GetTicks() - a->autosave_at > 60000)
		{
			char error[256];
			if (ms_project_save(&a->project, MS_DATA_DIR "/自动恢复.ymmusic", error, sizeof(error)))
				ms_status(a, "自动恢复副本已更新；请用保存按钮保存正式工程");
			else
				ms_status(a, error);
			a->autosave_at = SDL_GetTicks();
		}
		YMGUI_Refresh(a->ctx);
		SDL_LCD_Delay(10);
		++frame;
		if (a->frames > 0)
			--a->frames;
	}
	SDL_DelEventWatch(key_watch, a);
ui_done:
	if (a->worker)
	{
		SDL_WaitThread(a->worker, NULL);
		a->worker = NULL;
		free(a->job_asset.pcm);
	}
	YMGUI_Inject_SetKeyFilter(NULL, NULL);
	YMGUI_Inject_SetCtx(NULL);
	SDL_LCD_SetCloseRequestCb(NULL, NULL);
	ms_transport_close(&a->transport);
	YMGUI_Font_SetFallback(NULL);
	if (blob)
	{
		fclose(blob);
		blob = NULL;
	}
	YMGUI_Free_CtxFree(a->ctx);
	a->ctx = NULL;
	SDL_LCD_Destroy();
	GY_free1(disp.buf1);
done:
	ms_project_free(&a->project);
	ms_sounds_free(&a->sounds);
	free(a->gesture);
	free(a->history);
	free(a);
	if (!result)
		puts("music_studio 正常退出");
	return result;
}
