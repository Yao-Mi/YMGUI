#include "YMGUI_PubDefine.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_DrawFill.h"
#include "YMGUI_Label.h"
#include "YMGUI_Button.h"
#include "YMGUI_Checkbox.h"
#include "YMGUI_TextInput.h"
#include "YMGUI_Bar.h"
#include "YMGUI_Anim.h"
#include "SDL_LCD.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W 320
#define H 480
#define MAX_TASKS 12
#define PAGE_SIZE 4
#define RGB(r,g,b) GY_ARGB(0xFF,(r),(g),(b))

typedef struct { char title[32]; uint8 done; } Task;
static Task tasks[MAX_TASKS] = {
    {"Plan weekly goals", 1}, {"Review UI screens", 0},
    {"Polish interactions", 0}, {"Share the prototype", 0},
    {"Check animations", 0}, {"Write release notes", 0}
};
static int count = 6, page = 0, selected = -1;
static GYCTX ctx;
static GYOBJ input, progress, progress_text, page_text, empty_text;
static GYOBJ rows[PAGE_SIZE], checks[PAGE_SIZE], details[PAGE_SIZE];
static GYOBJ shade, sheet, sheet_title, sheet_state, sheet_action;
static GYOBJ prev_btn, next_btn;
static int sheet_open = 0;
static int failures = 0;

static void draw_shade(GYOBJ obj, GYSURFACE surface, const GYrect* area)
{
    (void)obj;
    YMGUI_Draw_Fill(surface, area, RGB(7, 15, 28), 125);
}

static GYOBJ label(GYOBJ parent, int x, int y, int w, const char* text, GYcolor color)
{
    GYOBJ o = YMGUI_Creat_Label_Creat(parent, x, y, w, 20);
    YMGUI_Label_SetText(o, text);
    YMGUI_Label_SetTextColor(o, color);
    return o;
}

static GYOBJ button(GYOBJ parent, int x, int y, int w, int h,
                    const char* text, GYbtn_clicked_cb callback, GYcolor color)
{
    GYOBJ o = YMGUI_Creat_Button_Creat(parent, x, y, w, h);
    YMGUI_Button_SetText(o, text);
    YMGUI_Button_SetColors(o, color, RGB(84, 106, 132));
    YMGUI_Button_SetClicked(o, callback);
    return o;
}

#if YMGUI_ANIM
static void progress_value(int32 value, void* user)
{
    (void)user;
    YMGUI_Bar_SetValue(progress, value);
}
static void sheet_closed(GYANIM animation, uint8 cancelled, void* user)
{
    (void)animation; (void)user;
    if (!cancelled && !sheet_open) {
        YMGUI_Obj_SetHidden(sheet, 1);
        YMGUI_Obj_SetHidden(shade, 1);
    }
}
#endif

static void update_progress(void)
{
    char text[48];
    int done = 0;
    for (int i = 0; i < count; ++i) done += tasks[i].done;
    int target = count ? (done * 100 / count) : 0;
    snprintf(text, sizeof(text), "%d of %d done  |  %d%%", done, count, target);
    YMGUI_Label_SetText(progress_text, text);
#if YMGUI_ANIM
    if (!YMGUI_Anim_ValueTo(ctx, progress, YMGUI_Bar_GetValue(progress), target,
                            360, GY_ANIM_SMOOTH, progress_value, NULL))
#endif
        YMGUI_Bar_SetValue(progress, target);
}

static void refresh_rows(void)
{
    char text[32];
    int pages = (count + PAGE_SIZE - 1) / PAGE_SIZE;
    if (pages < 1) pages = 1;
    if (page >= pages) page = pages - 1;
    snprintf(text, sizeof(text), "PAGE %d / %d", page + 1, pages);
    YMGUI_Label_SetText(page_text, text);
    YMGUI_Obj_SetHidden(empty_text, count != 0);
    for (int slot = 0; slot < PAGE_SIZE; ++slot) {
        int index = page * PAGE_SIZE + slot;
        YMGUI_Obj_SetHidden(rows[slot], index >= count);
        if (index >= count) continue;
        YMGUI_Checkbox_SetText(checks[slot], tasks[index].title);
        YMGUI_Checkbox_SetChecked(checks[slot], tasks[index].done);
        YMGUI_Obj_SetBgColor(rows[slot], tasks[index].done ? RGB(29, 62, 55) : RGB(28, 38, 57));
    }
    update_progress();
}

static void close_sheet(void)
{
    if (!sheet_open) return;
    sheet_open = 0;
#if YMGUI_ANIM
    GYANIM a = YMGUI_Anim_MoveTo(sheet, 0, H, 210, GY_ANIM_EASE_IN);
    if (a && YMGUI_Anim_SetDone(ctx, a, sheet_closed, NULL)) return;
#endif
    sheet->area.y = H;
    YMGUI_Obj_SetHidden(sheet, 1);
    YMGUI_Obj_SetHidden(shade, 1);
}

static void open_sheet(int index)
{
    if (index < 0 || index >= count) return;
    selected = index;
    YMGUI_Label_SetText(sheet_title, tasks[index].title);
    YMGUI_Label_SetText(sheet_state, tasks[index].done ? "Status: completed" : "Status: in progress");
    YMGUI_Button_SetText(sheet_action, tasks[index].done ? "Mark open" : "Complete");
    sheet_open = 1;
    YMGUI_Obj_SetHidden(shade, 0);
    YMGUI_Obj_SetHidden(sheet, 0);
#if YMGUI_ANIM
    if (YMGUI_Anim_MoveTo(sheet, 0, 292, 260, GY_ANIM_EASE_OUT)) return;
#endif
    sheet->area.y = 292;
    YMGUI_Ctx_InvalidateArea(ctx, &(GYrect){0, 292, W, 188});
}

static void on_check(GYOBJ box, uint8 checked)
{
    for (int slot = 0; slot < PAGE_SIZE; ++slot) if (checks[slot] == box) {
        int index = page * PAGE_SIZE + slot;
        if (index >= count) return;
        tasks[index].done = checked;
#if YMGUI_ANIM
        if (!YMGUI_Anim_BgColorTo(rows[slot], checked ? RGB(29, 62, 55) : RGB(28, 38, 57),
                                  260, GY_ANIM_SMOOTH))
#endif
            YMGUI_Obj_SetBgColor(rows[slot], checked ? RGB(29, 62, 55) : RGB(28, 38, 57));
        update_progress();
        return;
    }
}

static void on_detail(GYOBJ btn)
{
    for (int slot = 0; slot < PAGE_SIZE; ++slot)
        if (details[slot] == btn) { open_sheet(page * PAGE_SIZE + slot); return; }
}

static void add_task(void)
{
    const char* text = YMGUI_TextInput_GetText(input);
    while (*text == ' ') ++text;
    if (!*text || count >= MAX_TASKS) return;
    snprintf(tasks[count].title, sizeof(tasks[count].title), "%s", text);
    tasks[count].done = 0;
    ++count;
    page = (count - 1) / PAGE_SIZE;
    YMGUI_TextInput_SetText(input, "");
    refresh_rows();
#if YMGUI_ANIM
    int slot = (count - 1) % PAGE_SIZE;
    YMGUI_Obj_SetBgColor(rows[slot], RGB(45, 110, 98));
    YMGUI_Anim_BgColorTo(rows[slot], RGB(28, 38, 57), 400, GY_ANIM_SMOOTH);
#endif
}
static void on_add(GYOBJ btn) { (void)btn; add_task(); }
static void on_submit(GYOBJ ti, const char* text) { (void)ti; (void)text; add_task(); }
static void on_prev(GYOBJ btn) { (void)btn; if (page > 0) { --page; refresh_rows(); } }
static void on_next(GYOBJ btn) { (void)btn; if ((page + 1) * PAGE_SIZE < count) { ++page; refresh_rows(); } }
static void on_close(GYOBJ btn) { (void)btn; close_sheet(); }
static void on_action(GYOBJ btn)
{
    (void)btn;
    if (selected >= 0 && selected < count) {
        tasks[selected].done = !tasks[selected].done;
        refresh_rows();
    }
    close_sheet();
}
static void on_delete(GYOBJ btn)
{
    (void)btn;
    if (selected >= 0 && selected < count) {
        for (int i = selected; i + 1 < count; ++i) tasks[i] = tasks[i + 1];
        --count;
        refresh_rows();
    }
    selected = -1;
    close_sheet();
}

static void build_ui(void)
{
    GYOBJ root = ctx->root;
    YMGUI_Obj_SetBgColor(root, RGB(15, 23, 38));
    label(root, 18, 10, 190, "9:41  |  POCKET TASKS", RGB(126, 153, 177));
    label(root, 18, 40, 230, "Today", RGB(239, 248, 251));
    label(root, 18, 64, 270, "Make room for what matters.", RGB(151, 170, 191));

    GYOBJ card = YMGUI_Creat_Obj_Creat(root, 14, 94, 292, 84);
    YMGUI_Obj_SetBgColor(card, RGB(28, 54, 69));
    label(card, 14, 9, 245, "YOUR MOMENTUM", RGB(102, 218, 193));
    progress_text = label(card, 14, 31, 250, "", RGB(235, 246, 246));
    progress = YMGUI_Creat_Bar_Creat(card, 14, 62, 264, 9);
    YMGUI_Bar_SetRange(progress, 0, 100);
    YMGUI_Bar_SetColors(progress, RGB(50, 79, 89), RGB(89, 220, 179));

    label(root, 18, 191, 245, "ADD A TASK", RGB(117, 145, 173));
    input = YMGUI_Creat_TextInput_Creat(root, 14, 215, 228, 34, 31);
    YMGUI_TextInput_SetSubmitted(input, on_submit);
    button(root, 250, 215, 56, 34, "+", on_add, RGB(51, 151, 130));
    label(root, 18, 263, 240, "TODAY'S LIST", RGB(117, 145, 173));
    empty_text = label(root, 34, 331, 260, "All clear. Add your first task!", RGB(154, 178, 194));
    for (int i = 0; i < PAGE_SIZE; ++i) {
        rows[i] = YMGUI_Creat_Obj_Creat(root, 14, 288 + i * 36, 292, 32);
        checks[i] = YMGUI_Creat_Checkbox_Creat(rows[i], 8, 4, 223, 24);
        YMGUI_Checkbox_SetChanged(checks[i], on_check);
        details[i] = button(rows[i], 241, 3, 46, 26, "...", on_detail, RGB(42, 61, 79));
    }
    prev_btn = button(root, 14, 436, 64, 29, "<", on_prev, RGB(34, 55, 74));
    page_text = label(root, 116, 442, 130, "", RGB(126, 153, 177));
    next_btn = button(root, 242, 436, 64, 29, ">", on_next, RGB(34, 55, 74));
    (void)prev_btn; (void)next_btn;

    shade = YMGUI_Creat_Obj_Creat(YMGUI_Ctx_GetTopLayer(ctx), 0, 0, W, H);
    shade->draw_cb = draw_shade;
    sheet = YMGUI_Creat_Obj_Creat(YMGUI_Ctx_GetTopLayer(ctx), 0, H, W, 188);
    YMGUI_Obj_SetBgColor(sheet, RGB(29, 45, 64));
    label(sheet, 18, 14, 260, "TASK DETAILS", RGB(99, 217, 188));
    sheet_title = label(sheet, 18, 47, 280, "", RGB(241, 249, 252));
    sheet_state = label(sheet, 18, 75, 280, "", RGB(157, 179, 198));
    sheet_action = button(sheet, 16, 112, 132, 36, "Complete", on_action, RGB(49, 147, 125));
    button(sheet, 157, 112, 71, 36, "Delete", on_delete, RGB(133, 68, 79));
    button(sheet, 236, 112, 68, 36, "Close", on_close, RGB(58, 77, 97));
    YMGUI_Obj_SetHidden(sheet, 1);
    YMGUI_Obj_SetHidden(shade, 1);
    refresh_rows();
}

static void selftest(void)
{
    YMGUI_TextInput_SetText(input, "A new task");
    add_task();
    if (count != 7 || strcmp(tasks[6].title, "A new task") || page != 1) ++failures;
    YMGUI_Inject_Pointer(34, 374, 1);
    YMGUI_Inject_Pointer(34, 374, 0);
    if (!tasks[6].done) ++failures;
    YMGUI_Inject_Pointer(272, 374, 1);
    YMGUI_Inject_Pointer(272, 374, 0);
    if (!sheet_open || selected != 6) ++failures;
    YMGUI_Inject_Tick(300);
    YMGUI_Inject_Pointer(191, 422, 1);
    YMGUI_Inject_Pointer(191, 422, 0);
    if (count != 6 || sheet_open || page != 1) ++failures;
    YMGUI_Inject_Tick(220);
    YMGUI_Inject_Pointer(44, 450, 1);
    YMGUI_Inject_Pointer(44, 450, 0);
    if (page != 0) ++failures;
    printf("pocket_tasks selftest: %s\n", failures ? "FAIL" : "OK");
}

int main(int argc, char** argv)
{
    int max_frames = -1, test = 0, frame = 0;
    if (argc > 1) {
        if (!strcmp(argv[1], "--selftest")) { test = 1; max_frames = 25; }
        else if (!strcmp(argv[1], "--sheet")) { max_frames = 40; }
        else max_frames = atoi(argv[1]);
    }
    GYdisp disp = {0};
    disp.hor_res = W; disp.ver_res = H; disp.buf_px_cnt = W * 48;
    disp.buf1 = (GYpx*)GY_malloc1(disp.buf_px_cnt * sizeof(GYpx));
    if (!disp.buf1 || SDL_LCD_Init(&disp, 1) != 0) return 1;
    SDL_LCD_SetTitle("Pocket Tasks - YMGUI");
    ctx = YMGUI_Creat_Ctx_Creat(&disp, W, H);
    if (!ctx) return 1;
    YMGUI_Inject_SetCtx(ctx);
    build_ui();
    if (test) selftest();
    else if (argc > 1 && !strcmp(argv[1], "--sheet")) open_sheet(1);
    while (SDL_LCD_PumpEvents()) {
        YMGUI_Refresh(ctx);
        SDL_LCD_Delay(16);
        if (max_frames > 0 && ++frame >= max_frames) break;
    }
    YMGUI_Free_CtxFree(ctx);
    SDL_LCD_Destroy();
    GY_free1(disp.buf1);
    return failures ? 1 : 0;
}
