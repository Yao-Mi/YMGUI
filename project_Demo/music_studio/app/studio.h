#ifndef MS_STUDIO_H
#define MS_STUDIO_H
#include "model/project.h"
#include "audio/audio.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Font.h"
#include "YMGUI_Button.h"
#include "YMGUI_Label.h"
#include "YMGUI_Slider.h"
#include "YMGUI_Dropdown.h"
#include "YMGUI_TextInput.h"
#include "YMGUI_FileDialog.h"
#include "YMGUI_MsgBox.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_DrawFill.h"
#include "YMGUI_Event.h"

#define MS_W 1680
#define MS_H 1050
#define MS_RGB(c) (0xff000000u | (c))
#define MS_BG 0x202327
#define MS_PANEL 0x2a2e33
#define MS_TEXT 0xe1e5e9
#define MS_DIM 0x959da7
#define MS_ACCENT 0x70c6ae
#define MS_TIMEX 408
#define MS_TIMEY 182
#define MS_ROWH 34
#define MS_GRID_X 400
#define MS_GRID_Y 606
#define MS_GRID_WIDTH (MS_W - MS_GRID_X - 24)
#define MS_DRUM_ROW 36
#define MS_KEY_ROW 31

typedef enum
{
	A_NEW,
	A_OPEN,
	A_SAVE,
	A_SAVE_AS,
	A_IMPORT,
	A_EXPORT,
	A_EXPORT_LOOP,
	A_UNDO,
	A_REDO,
	A_HELP,
	A_PLAY,
	A_STOP,
	A_LOOP,
	A_METRO,
	A_MODE,
	A_BEATS,
	A_ADD_DRUM,
	A_ADD_SYNTH,
	A_ADD_AUDIO,
	A_DEL_TRACK,
	A_UP,
	A_DOWN,
	A_REPEAT,
	A_SPLIT,
	A_DELETE,
	A_ZOOM_IN,
	A_ZOOM_OUT,
	A_LOOP_START,
	A_LOOP_END,
	A_DRUM,
	A_PIANO,
	A_MIXER,
	A_PAT_NEW,
	A_PAT_COPY,
	A_PAT_UNIQUE,
	A_PAT_LENGTH,
	A_PAT_CLEAR,
	A_LIBRARY_PAT,
	A_LIBRARY_AUDIO,
	A_APPEND,
	A_SAMPLE,
	A_SAMPLE_RESET,
	A_DRUM_MUTE,
	A_DRUM_SOLO,
	A_NOTE_SHORT,
	A_NOTE_LONG,
	A_OCT_DOWN,
	A_OCT_UP,
	A_INSTRUMENT,
	A_FADE_IN,
	A_FADE_OUT,
	A_SNAP,
	A_QUANTIZE,
	A_PREVIEW,
	A_NOTE_SELECT,
	A_NOTE_DELETE
} MsAction;
typedef struct MsApp
{
	MsProject project;
	MsHistory* history;
	MsSounds sounds;
	MsTransport transport;
	GYCTX ctx;
	GYOBJ drag_ghost;
	GYOBJ timeline, library, editor, status, clock_label, title, detail;
	GYOBJ track_name, pattern_name, bpm_input, volume, pan, velocity, master, clip_gain;
	GYOBJ instrument, sound_mode, instrument_label, fade_in, fade_out, piano_grid;
	GYOBJ drum_pan_slider, drum_pan_label, velocity_label;
	GYOBJ dialog, msgbox;
	GYOBJ buttons[96];
	MsAction actions[96];
	int button_count;
	int track, pattern, asset, clip, drum, step, note, tab, library_tab;
	int note_length, note_velocity, octave, snap, dirty, quit, syncing, note_grid;
	/* 框选为会话状态，不写入工程或撤销历史。 */
	int select_notes, selection_pattern, box_x, box_y;
	uint8_t note_selected[MS_NOTES], selection_before[MS_NOTES];
	int drag, drag_x, drag_y, drag_index, drag_mode;
	int drag_active, drop_track, drop_tick, drop_length, drop_valid;
	MsClip drag_clip;
	MsClip clipboard;
	int clipboard_valid;
	MsNote drag_note;
	MsSong* gesture;
	double zoom;
	double editor_zoom, editor_scroll;
	int scroll;
	char path[MS_PATH], directory[MS_PATH], pending[MS_PATH];
	int dialog_action, confirm_action, overwrite_action;
	int ctrl, shift, frames, selftest, export_loop, import_drum;
	SDL_Thread* worker;
	SDL_atomic_t job_done;
	int job_action, job_ok;
	char job_path[MS_PATH], job_error[256];
	MsAsset job_asset;
	uint32_t autosave_at;
} MsApp;

void ms_action(MsApp* a, MsAction action);
void ms_changed(MsApp* a);
void ms_checkpoint(MsApp* a);
void ms_ui_build(MsApp* a);
void ms_ui_refresh(MsApp* a);
int ms_instrument_popup_open(MsApp* a);
void ms_status(MsApp* a, const char* text);
void ms_dialog(MsApp* a, MsAction action);
void ms_confirm(MsApp* a, int action, const char* text);
int ms_close_request(void* user);
void ms_job_start(MsApp* a, int action, const char* path);
void ms_job_poll(MsApp* a);
int ms_save(MsApp* a, const char* path);
int ms_load(MsApp* a, const char* path);
void ms_timeline_zoom_at(MsApp* a, int x, int steps);
void ms_editor_view_clamp(MsApp* a);
void ms_timeline_draw(GYOBJ o, GYSURFACE s, const GYrect* r);
void ms_timeline_event(GYOBJ o, GYEvent event);
int ms_piano_grid_ticks(int index);
int ms_piano_step(const MsApp* a);
void ms_notes_clear_selection(MsApp* a);
int ms_note_selected(const MsApp* a, int index);
int ms_notes_selection_count(const MsApp* a);
void ms_editor_draw(GYOBJ o, GYSURFACE s, const GYrect* r);
void ms_editor_event(GYOBJ o, GYEvent event);
void ms_drag_ghost_draw(GYOBJ o, GYSURFACE s, const GYrect* r);
void ms_library_draw(GYOBJ o, GYSURFACE s, const GYrect* r);
void ms_library_event(GYOBJ o, GYEvent event);
void ms_select_pattern(MsApp* a, int pattern);
void ms_add_source(MsApp* a, int track, int tick);
int ms_ui_selftest(MsApp* a);
void ms_fill(GYSURFACE s, int x, int y, int w, int h, unsigned color);
void ms_text(GYSURFACE s, int x, int y, const char* text, unsigned color);
void ms_border(GYSURFACE s, int x, int y, int w, int h, unsigned color);
GYOBJ ms_panel(MsApp* a, int x, int y, int w, int h, GYobj_draw_cb draw, GYobj_event_cb event);
GYOBJ ms_label(MsApp* a, int x, int y, int w, const char* text, unsigned color);
GYOBJ ms_button(MsApp* a, int x, int y, int w, const char* text, MsAction action);
int ms_clamp(int v, int lo, int hi);
#endif
