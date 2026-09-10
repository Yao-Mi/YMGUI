#ifndef STUDIO_H
#define STUDIO_H
#include "model/project.h"
#include "model/history.h"
#include "media/engine.h"
#include "media/export.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Bind.h"
#include "YMGUI_Image.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Event.h"
#include <stdint.h>
#define ST_W 1600
#define ST_H 1000
#define ST_LEFT 300
#define ST_RIGHT 1308
#define ST_TL_Y 666
#define ST_HEADER 156
#define ST_RULER 28
#define ST_ROW 66
#define ST_TL_H (ST_H - ST_TL_Y - 42)
#define ST_WORK_H (ST_TL_Y - 122)
#define ST_VISIBLE_TRACKS ((ST_TL_H - ST_RULER) / ST_ROW)
#define ST_BIN_ROWS ((ST_WORK_H - 120) / 65)
#define ST_RGB(v) GY_ARGB(255, ((v) >> 16) & 255, ((v) >> 8) & 255, (v) & 255)
typedef enum
{
	ACT_IMPORT,
	ACT_OPEN,
	ACT_SAVE,
	ACT_UNDO,
	ACT_REDO,
	ACT_PLAY,
	ACT_PREV,
	ACT_NEXT,
	ACT_HOME,
	ACT_END,
	ACT_APPEND,
	ACT_SPLIT,
	ACT_LIFT,
	ACT_RIPPLE,
	ACT_TRACK,
	ACT_SOURCE,
	ACT_PROJECT,
	ACT_ZOOM_IN,
	ACT_ZOOM_OUT,
	ACT_FIT,
	ACT_IN,
	ACT_OUT,
	ACT_EXPORT,
	ACT_EXPORT_CANCEL
} StAction;
typedef struct
{
	char path[1024];
	uint16_t pixels[160 * 90];
	GYimg image;
} StThumb;
typedef struct Studio
{
	StProject project;
	StHistory history;
	StEngine* engine;
	StExport* exporter;
	GYsubject export_running, export_progress, export_text;
	GYOBJ export_button, export_cancel, export_label, export_bar;
	char export_buffer[64];
	StExportState export_last_state;
	int export_cancelling;
	GYCTX ctx;
	GYdisp display;
	GYsubject position, source_position, playing, mode, zoom, scroll, revision, status;
	GYobserver* observers[7];
	GYOBJ preview, bin, timeline, head, play, seek, timecode, properties, title, scrollbar;
	GYobj_event_cb seek_event;
	GYOBJ source_tab, project_tab, undo, redo, dialog, msgbox;
	GYOBJ action_buttons[64];
	StAction action_ids[64];
	int action_count;
	GYimg preview_image;
	uint16_t pixels[ST_PIXELS], incoming[ST_PIXELS];
	struct
	{
		char path[1024];
		int frame, state;
		uint64_t used;
		GYimg image;
		uint16_t pixels[80 * 45];
	} clip_thumbs[32];
	uint64_t thumb_clock;
	uint32_t thumb_tick;
	StThumb thumbs[ST_MEDIA];
	int thumb_count;
	int selected_media, selected_track, selected_clip, bin_scroll, track_scroll;
	int drag_kind, press_x, press_y, press_frame, drag_id, drag_track, drag_start, drag_in, drag_length;
	int ghost_track, ghost_at, ghost_length, ghost_valid, drag_moved, scrubbing;
	uint64_t wanted_serial, min_serial, shown_serial;
	uint32_t anchor_tick, request_tick;
	int anchor_frame, request_dirty, internal_change;
	int dirty, quit, dialog_action, shift, alt, modal_action, pending_imports, preview_ok;
	uint32_t edge_tick;
	int proxy_states[ST_MEDIA];
	char status_text[256], project_path[1024], pending_path[1024];
	double last_decode_ms;
} Studio;
Studio* studio_create(void);
void studio_destroy(Studio* s);
void studio_tick(Studio* s);
int studio_pump(Studio* s);
void studio_import(Studio* s, const char* path);
void studio_export_bind(Studio* s);
void studio_export_poll(Studio* s);
int studio_export_start(Studio* s, const char* path, int overwrite);
void studio_action(Studio* s, StAction action);
int studio_edit(Studio* s, StEdit e);
void studio_changed(Studio* s);
void studio_status(Studio* s, const char* message);
void studio_seek(Studio* s, int frame, int final);
int studio_position(Studio* s);
int studio_duration(Studio* s);
void studio_mode(Studio* s, int source);
void studio_request(Studio* s, int final);
void studio_ui_build(Studio* s);
void studio_ui_refresh(Studio* s);
void studio_bind(Studio* s);
void studio_unbind(Studio* s);
void studio_timeline_draw(GYOBJ obj, GYSURFACE surface, const GYrect* area);
void studio_timeline_event(GYOBJ obj, GYEvent event);
void studio_head_apply(GYOBJ obj, const GYval* value);
int studio_frame_at(Studio* s, int x);
int studio_track_at(Studio* s, int y);
void studio_bin_draw(GYOBJ obj, GYSURFACE surface, const GYrect* area);
void studio_bin_event(GYOBJ obj, GYEvent event);
GYIMG studio_thumbnail(Studio* s, int media);
GYIMG studio_clip_thumbnail(Studio* s, int media, int frame);
void studio_thumbnail_tick(Studio* s);
GYOBJ studio_panel(GYOBJ p, int x, int y, int w, int h, unsigned color);
GYOBJ studio_label(GYOBJ p, int x, int y, int w, int h, const char* text, unsigned color);
GYOBJ studio_button(GYOBJ p, int x, int y, int w, int h, const char* text, StAction action);
void studio_font_open(void);
void studio_font_close(void);
void studio_dialog_init(Studio* s);
void studio_dialog_show(Studio* s, int action);
void studio_confirm(Studio* s, int action, const char* text);
void studio_timecode(char* dst, size_t size, int frame);
static inline Studio* studio_of(GYOBJ obj)
{
	return obj->ctx->root->user_data;
}
#endif
