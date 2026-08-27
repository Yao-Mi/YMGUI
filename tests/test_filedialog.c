#include "YMGUI_PubDefine.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Event.h"
#include "YMGUI_FileDialog.h"
#include "YMGUI_Mem.h"
#include <stdio.h>
#include <string.h>

#define SCR_W 360
#define SCR_H 260

static int fails;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); fails++; } } while (0)
static void flush(GYdisp* d, const GYrect* a, const GYpx* p) { (void)d; (void)a; (void)p; }

typedef struct { const char* path; int pos; } MockDir;
static MockDir dirs[2];
static int made_dir;
static void* mockOpen(void* u, const char* path)
{
	(void)u;
	if (strcmp(path, "/") != 0 && strcmp(path, "/docs") != 0) return NULL;
	MockDir* d = &dirs[strcmp(path, "/") == 0 ? 0 : 1]; d->path = path; d->pos = 0; return d;
}
static int mockRead(void* u, void* handle, GYfiledialog_entry* e)
{
	(void)u; MockDir* d = (MockDir*)handle;
	const char* name = NULL; uint8 is_dir = 0;
	if (strcmp(d->path, "/") == 0)
	{
		if (d->pos == 0) { name = "."; is_dir = 1; }
		else if (d->pos == 1) { name = "docs"; is_dir = 1; }
		else if (d->pos == 2) name = "a.txt";
	}
	else if (d->pos == 0) name = "guide.md";
	d->pos++;
	if (!name) return 0;
	snprintf(e->name, e->name_cap, "%s", name); e->is_dir = is_dir; e->size = 12; return 1;
}
static void mockClose(void* u, void* d) { (void)u; (void)d; }
static uint8 mockStat(void* u, const char* p, uint8* exists, uint8* is_dir)
{
	(void)u; *exists = 1; *is_dir = 0;
	if (strcmp(p, "/") == 0 || strcmp(p, "/docs") == 0 || strcmp(p, "/newdir") == 0) *is_dir = 1;
	else if (strcmp(p, "/a.txt") != 0 && strcmp(p, "/docs/guide.md") != 0 && strcmp(p, "/old.txt") != 0) *exists = 0;
	return 1;
}
static uint8 mockMkdir(void* u, const char* p) { (void)u; if (strcmp(p, "/newdir") != 0) return 0; made_dir++; return 1; }

static int result_count, accepted;
static char result_path[80];
static void resultCb(GYOBJ fd, uint8 ok, const char* path, void* u)
{ (void)fd; (void)u; result_count++; accepted = ok; snprintf(result_path, sizeof(result_path), "%s", path ? path : ""); }
static uint8 overwrite_no(GYOBJ fd, const char* p, void* u) { (void)fd; (void)p; (void)u; return 0; }
static uint8 overwrite_yes(GYOBJ fd, const char* p, void* u) { (void)fd; (void)p; (void)u; return 1; }
static void click(GYCTX ctx, GYcoord x, GYcoord y)
{ YMGUI_Event_Pointer(ctx, x, y, 1); YMGUI_Event_Pointer(ctx, x, y, 0); }

int main(void)
{
	GYdisp disp = {0}; disp.hor_res = SCR_W; disp.ver_res = SCR_H; disp.buf_px_cnt = SCR_W * 32;
	disp.buf1 = (GYpx*)GY_malloc1(disp.buf_px_cnt * sizeof(GYpx)); disp.flush_cb = flush;
	GYCTX ctx = YMGUI_Creat_Ctx_Creat(&disp, SCR_W, SCR_H);
	CHECK(YMGUI_Creat_FileDialog_Creat(ctx, SCR_W + 1, 230, 64, 24, 8) == NULL,
		"reject dialog wider than viewport");
	CHECK(YMGUI_Creat_FileDialog_Creat(ctx, 320, 230, 64, 24,
		GY_FILEDIALOG_MAX_ENTRIES + 1) == NULL, "reject row height overflow");
	GYOBJ fd = YMGUI_Creat_FileDialog_Creat(ctx, 320, 230, 64, 24, 8);
	CHECK(fd != NULL, "created"); CHECK(fd && fd->parent == ctx->top_layer, "attached to top layer");
	CHECK(!YMGUI_FileDialog_IsShown(fd), "initially hidden");
	GYfiledialog_fs fs = {mockOpen, mockRead, mockClose, mockStat, mockMkdir};
	YMGUI_FileDialog_SetFS(fd, &fs, NULL); YMGUI_FileDialog_SetResultCb(fd, resultCb, NULL);

	CHECK(YMGUI_FileDialog_Show(fd, GY_FILE_DIALOG_OPEN_FILE, "/", NULL), "show open");
	CHECK(YMGUI_FileDialog_GetEntryCount(fd) == 2, "dot entry filtered");
	CHECK(YMGUI_FileDialog_GetEntry(fd, 0)->is_dir, "first entry directory");
	CHECK(YMGUI_FileDialog_ActivateIndex(fd, 0), "activate directory");
	CHECK(strcmp(YMGUI_FileDialog_GetPath(fd), "/docs") == 0, "navigated into docs");
	CHECK(YMGUI_FileDialog_Up(fd), "navigate up");
	CHECK(strcmp(YMGUI_FileDialog_GetPath(fd), "/") == 0, "up returns root");
	CHECK(YMGUI_FileDialog_SelectIndex(fd, 0), "select child directory for Go");
	CHECK(YMGUI_FileDialog_Go(fd), "Go enters selected child directory");
	CHECK(strcmp(YMGUI_FileDialog_GetPath(fd), "/docs") == 0, "Go is forward counterpart of Up");
	CHECK(YMGUI_FileDialog_Up(fd), "return root after Go test");
	CHECK(YMGUI_FileDialog_SelectIndex(fd, 1), "select file");
	CHECK(strcmp(YMGUI_FileDialog_GetName(fd), "a.txt") == 0, "file selection fills name");
	CHECK(YMGUI_FileDialog_Confirm(fd), "open existing file");
	CHECK(result_count == 1 && accepted && strcmp(result_path, "/a.txt") == 0, "open result path");

	CHECK(YMGUI_FileDialog_Show(fd, GY_FILE_DIALOG_OPEN_FILE, "/", "missing.txt"), "show missing open");
	CHECK(!YMGUI_FileDialog_Confirm(fd), "open rejects missing file");
	CHECK(YMGUI_FileDialog_IsShown(fd), "rejected dialog remains open");

	YMGUI_FileDialog_SetOverwriteCb(fd, overwrite_no);
	CHECK(YMGUI_FileDialog_Show(fd, GY_FILE_DIALOG_SAVE_FILE, "/", "old.txt"), "show save");
	CHECK(!YMGUI_FileDialog_Confirm(fd), "save existing requires overwrite confirmation");
	YMGUI_FileDialog_SetOverwriteCb(fd, overwrite_yes);
	CHECK(YMGUI_FileDialog_Confirm(fd), "save accepted after overwrite confirmation");
	CHECK(result_count == 2 && strcmp(result_path, "/old.txt") == 0, "save result path");

	CHECK(YMGUI_FileDialog_Show(fd, GY_FILE_DIALOG_SELECT_DIRECTORY, "/", NULL), "show select directory");
	CHECK(YMGUI_FileDialog_SelectIndex(fd, 0), "select docs directory");
	CHECK(YMGUI_FileDialog_Confirm(fd), "confirm selected directory");
	CHECK(result_count == 3 && strcmp(result_path, "/docs") == 0, "selected directory path");

	//FileDialog 的树:首行 docs 点三角展开,第二行选中子文件,确认得到完整层级路径。
	CHECK(YMGUI_FileDialog_Show(fd, GY_FILE_DIALOG_OPEN_FILE, "/", NULL), "show tree browsing");
	click(ctx, 32, 98);  //card(20,15)+tree(8,72)+marker(4),展开 docs
	click(ctx, 70, 120); //展开后的第二行 guide.md
	CHECK(strcmp(YMGUI_FileDialog_GetName(fd), "guide.md") == 0, "tree child selection fills full name");
	CHECK(YMGUI_FileDialog_Confirm(fd), "confirm nested tree file");
	CHECK(result_count == 4 && strcmp(result_path, "/docs/guide.md") == 0, "nested tree result path");

	//真实键盘编辑路径栏后点击 Go，验证按钮回调和路径提交链路。
	CHECK(YMGUI_FileDialog_Show(fd, GY_FILE_DIALOG_OPEN_FILE, "/", NULL), "show for editable path");
	YMGUI_Event_Key(ctx, GY_KEY_SEL_ALL);
	for (const char* p = "/docs"; *p; p++) YMGUI_Event_Key(ctx, (uint8)*p);
	click(ctx, 100, 68); //card(20,15)+Go(68,39)
	CHECK(strcmp(YMGUI_FileDialog_GetPath(fd), "/docs") == 0, "Go submits edited path");
	CHECK(YMGUI_FileDialog_GetEntryCount(fd) == 1, "Go refreshes destination tree");
	YMGUI_FileDialog_Close(fd);

	//路径栏按 Enter:先退出编辑态,再提交路径并刷新树。
	CHECK(YMGUI_FileDialog_Show(fd, GY_FILE_DIALOG_OPEN_FILE, "/", NULL), "show for path Enter");
	YMGUI_Event_Key(ctx, GY_KEY_SEL_ALL);
	for (const char* p = "/docs"; *p; p++) YMGUI_Event_Key(ctx, (uint8)*p);
	CHECK(ctx->focus_obj && (ctx->focus_obj->state & GY_STATE_Editing), "typing enters path edit mode");
	YMGUI_Event_Key(ctx, GY_KEY_ENTER);
	CHECK(strcmp(YMGUI_FileDialog_GetPath(fd), "/docs") == 0, "Enter submits edited path");
	CHECK(ctx->focus_obj && !(ctx->focus_obj->state & GY_STATE_Editing), "Enter exits path edit mode");
	CHECK(YMGUI_FileDialog_GetEntryCount(fd) == 1, "Enter refreshes destination tree");
	YMGUI_FileDialog_Close(fd);

	CHECK(YMGUI_FileDialog_Show(fd, GY_FILE_DIALOG_SAVE_FILE, "/", ""), "show for mkdir");
	CHECK(ctx->focus_obj != NULL, "show takes keyboard focus");
	CHECK(YMGUI_FileDialog_NewDirectory(fd, "newdir"), "new directory callback");
	CHECK(made_dir == 1, "mkdir called once");
	CHECK(!YMGUI_FileDialog_NewDirectory(fd, "../bad"), "reject path separators in folder name");
	YMGUI_FileDialog_Close(fd); CHECK(!YMGUI_FileDialog_IsShown(fd), "close hides");
	CHECK(ctx->focus_obj == NULL, "close releases hidden input focus");

	YMGUI_Free_CtxFree(ctx); GY_free1(disp.buf1);
	if (fails) { printf("test_filedialog: %d failure(s)\n", fails); return 1; }
	printf("test_filedialog: all passed\n"); return 0;
}
