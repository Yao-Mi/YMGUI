#include "YMGUI_PubDefine.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Button.h"
#include "YMGUI_Label.h"
#include "YMGUI_FileDialog.h"
#include "SDL_LCD.h"
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCR_W 420
#define SCR_H 300
#define BAND_H 48

static GYOBJ dialog, status_label;
static void* posixOpen(void* u, const char* path) { (void)u; return opendir(path); }
static int posixRead(void* u, void* h, GYfiledialog_entry* e)
{
	(void)u; struct dirent* de = readdir((DIR*)h); if (!de) return 0;
	snprintf(e->name, e->name_cap, "%s", de->d_name); e->size = 0;
	e->is_dir = (de->d_type == DT_DIR);
	if (de->d_type == DT_UNKNOWN)
	{
		struct stat st;
		if (fstatat(dirfd((DIR*)h), de->d_name, &st, AT_SYMLINK_NOFOLLOW) == 0)
			e->is_dir = S_ISDIR(st.st_mode);
	}
	return 1;
}
static void posixClose(void* u, void* h) { (void)u; closedir((DIR*)h); }
static uint8 posixStat(void* u, const char* path, uint8* exists, uint8* is_dir)
{
	(void)u; struct stat st;
	if (stat(path, &st) != 0) { *exists = 0; *is_dir = 0; return 1; }
	*exists = 1; *is_dir = S_ISDIR(st.st_mode); return 1;
}
static uint8 posixMkdir(void* u, const char* path) { (void)u; return mkdir(path, 0755) == 0; }
static uint8 allowOverwrite(GYOBJ fd, const char* path, void* u)
{ (void)fd; (void)u; printf("overwrite confirmed by demo: %s\n", path); return 1; }
static void result(GYOBJ fd, uint8 accepted, const char* path, void* u)
{
	(void)u; char text[64];
	const char* action = "Cancelled";
	if (accepted)
	{
		GYfiledialog_mode mode = YMGUI_FileDialog_GetMode(fd);
		action = mode == GY_FILE_DIALOG_OPEN_FILE ? "Opened" :
			mode == GY_FILE_DIALOG_SAVE_FILE ? "Save target" : "Directory";
	}
	snprintf(text, sizeof(text), "%s: %s", action, path ? path : "");
	YMGUI_Label_SetText(status_label, text); printf("%s\n", text);
}
static void showMode(GYfiledialog_mode mode)
{
	char cwd[256]; if (!getcwd(cwd, sizeof(cwd))) snprintf(cwd, sizeof(cwd), ".");
	YMGUI_FileDialog_Show(dialog, mode, cwd, mode == GY_FILE_DIALOG_SAVE_FILE ? "output.txt" : "");
}
static void openClick(GYOBJ b) { (void)b; showMode(GY_FILE_DIALOG_OPEN_FILE); }
static void saveClick(GYOBJ b) { (void)b; showMode(GY_FILE_DIALOG_SAVE_FILE); }
static void dirClick(GYOBJ b) { (void)b; showMode(GY_FILE_DIALOG_SELECT_DIRECTORY); }

int main(int argc, char** argv)
{
	GYdisp disp = {0}; int max_frames = argc > 1 ? atoi(argv[1]) : -1, frame = 0;
	disp.hor_res = SCR_W; disp.ver_res = SCR_H; disp.buf_px_cnt = SCR_W * BAND_H;
	disp.buf1 = (GYpx*)GY_malloc1(disp.buf_px_cnt * sizeof(GYpx));
	if (SDL_LCD_Init(&disp, 1) != 0) return 1;
	GYCTX ctx = YMGUI_Creat_Ctx_Creat(&disp, SCR_W, SCR_H); YMGUI_Inject_SetCtx(ctx);
	YMGUI_Obj_SetBgColor(ctx->root, GY_ARGB(0xFF, 0x19, 0x1B, 0x20));
	GYOBJ title = YMGUI_Creat_Label_Creat(ctx->root, 16, 18, 388, 24);
	YMGUI_Label_SetText(title, "YMGUI FileDialog");
	GYOBJ b1 = YMGUI_Creat_Button_Creat(ctx->root, 20, 70, 116, 34);
	GYOBJ b2 = YMGUI_Creat_Button_Creat(ctx->root, 152, 70, 116, 34);
	GYOBJ b3 = YMGUI_Creat_Button_Creat(ctx->root, 284, 70, 116, 34);
	YMGUI_Button_SetText(b1, "Open file"); YMGUI_Button_SetClicked(b1, openClick);
	YMGUI_Button_SetText(b2, "Save file"); YMGUI_Button_SetClicked(b2, saveClick);
	YMGUI_Button_SetText(b3, "Select dir"); YMGUI_Button_SetClicked(b3, dirClick);
	status_label = YMGUI_Creat_Label_Creat(ctx->root, 20, 124, 380, 24);
	YMGUI_Label_SetText(status_label, "Choose a dialog mode");
	dialog = YMGUI_Creat_FileDialog_Creat(ctx, 400, 280, 511, 255, 128);
	GYfiledialog_fs fs = {posixOpen, posixRead, posixClose, posixStat, posixMkdir};
	YMGUI_FileDialog_SetFS(dialog, &fs, NULL); YMGUI_FileDialog_SetResultCb(dialog, result, NULL);
	YMGUI_FileDialog_SetOverwriteCb(dialog, allowOverwrite);
	while (SDL_LCD_PumpEvents())
	{
		YMGUI_Refresh(ctx); SDL_LCD_Delay(16);
		if (max_frames > 0 && ++frame >= max_frames) break;
	}
	YMGUI_Free_CtxFree(ctx); SDL_LCD_Destroy(); GY_free1(disp.buf1); return 0;
}
