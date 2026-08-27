#include "YMGUI_FileDialog.h"

#if YMGUI_FILEDIALOG

#include "YMGUI_Button.h"
#include "YMGUI_TextInput.h"
#include "YMGUI_TreeView.h"
#include "YMGUI_Event.h"
#include "YMGUI_DrawFill.h"
#include "YMGUI_Font.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Mem.h"
#include <string.h>

#define FD_PAD         8
#define FD_PATH_H     26
#define FD_TOOL_H     28
#define FD_STATUS_H   18
#define FD_NAME_H     26
#define FD_ACTION_H   28
#define FD_STATUS_MAX 96

typedef struct { char* name; } GYfd_node_meta;

typedef struct
{
	GYfiledialog_fs fs;
	void* fs_user;
	GYfiledialog_result_cb result_cb;
	GYfiledialog_overwrite_cb overwrite_cb;
	GYfiledialog_filter_cb filter_cb;
	void* cb_user;
	void* filter_user;
	GYfiledialog_mode mode;
	size_t path_cap, name_cap;
	uint16 entry_cap, entry_count, node_count;
	int32 selected;
	char *path, *joined, *old_path, *scratch_name, *names, *node_names;
	GYfiledialog_entry* entries;
	GYfd_node_meta* node_meta;
	GYTREENODE* root_nodes;
	GYTREENODE selected_node;
	char status[FD_STATUS_MAX];
	GYOBJ card, tree, path_input, name_input;
	GYOBJ btn_up, btn_go, btn_refresh, btn_mkdir, btn_ok, btn_cancel;
} GYfd_data;

static void copyText(char* dst, size_t cap, const char* src)
{
	if (!dst || cap == 0) return;
	if (!src) src = "";
	size_t n = 0;
	while (src[n] && n + 1 < cap) { dst[n] = src[n]; n++; }
	dst[n] = '\0';
}

static void setStatus(GYOBJ fd, const char* text)
{
	GYfd_data* d = (GYfd_data*)fd->user_data;
	copyText(d->status, sizeof(d->status), text);
	YMGUI_Obj_Invalidate(d->card);
}

static uint8 validName(const char* name)
{
	if (!name || !name[0]) return 0;
	if ((name[0] == '.' && !name[1]) ||
		(name[0] == '.' && name[1] == '.' && !name[2])) return 0;
	for (const char* p = name; *p; p++) if (*p == '/' || *p == '\\') return 0;
	return 1;
}

static uint8 appendName(char* out, size_t cap, const char* name)
{
	size_t a = strlen(out), b = strlen(name);
	uint8 slash = (a > 0 && out[a - 1] != '/') ? 1 : 0;
	if (a + slash + b >= cap) return 0;
	if (slash) out[a++] = '/';
	memcpy(out + a, name, b + 1);
	return 1;
}

static uint8 joinPath(GYfd_data* d, const char* base, const char* name)
{
	copyText(d->joined, d->path_cap + 1, base);
	return appendName(d->joined, d->path_cap + 1, name);
}

static uint8 entryVisible(GYOBJ fd, const char* parent_path,
		const GYfiledialog_entry* entry)
{
	GYfd_data* d = (GYfd_data*)fd->user_data;
	return !d->filter_cb || d->filter_cb(fd, d->mode, parent_path, entry,
		d->filter_user);
}

static void parentPath(char* path)
{
	size_t n = strlen(path);
	while (n > 1 && path[n - 1] == '/') path[--n] = '\0';
	size_t slash = n;
	while (slash > 0 && path[slash - 1] != '/') slash--;
	if (slash == 0) path[0] = '\0';
	else if (slash == 1) path[1] = '\0';
	else path[slash - 1] = '\0';
}

static const char* fullNodeName(GYTREENODE node)
{
	GYfd_node_meta* m = node ? (GYfd_node_meta*)YMGUI_TreeView_NodeUserPtr(node) : NULL;
	return (m && m->name) ? m->name : YMGUI_TreeView_NodeName(node);
}

static uint8 buildNodePath(GYfd_data* d, GYTREENODE node, char* out)
{
	if (!node) { copyText(out, d->path_cap + 1, d->path); return 1; }
	if (!buildNodePath(d, YMGUI_TreeView_NodeParent(node), out)) return 0;
	return appendName(out, d->path_cap + 1, fullNodeName(node));
}

static void backdropDraw(GYOBJ obj, GYSURFACE s, const GYrect* abs)
{ (void)obj; YMGUI_Draw_Fill(s, abs, GY_ARGB(0xA0, 0, 0, 0), 0xA0); }
static void swallowEvent(GYOBJ obj, GYEvent e) { (void)obj; (void)e; }

static void cardDraw(GYOBJ obj, GYSURFACE s, const GYrect* abs)
{
	GYfd_data* d = obj->parent ? (GYfd_data*)obj->parent->user_data : NULL;
	if (!d) return;
	YMGUI_Draw_Fill(s, abs, GY_ARGB(0xFF, 0x20, 0x22, 0x28), GY_OPA_COVER);
	GYcolor c = GY_ARGB(0xFF, 0x68, 0x70, 0x80);
	GYrect t = {abs->x, abs->y, abs->w, 1}, b = {abs->x, abs->y + abs->h - 1, abs->w, 1};
	GYrect l = {abs->x, abs->y, 1, abs->h}, r = {abs->x + abs->w - 1, abs->y, 1, abs->h};
	YMGUI_Draw_Fill(s, &t, c, GY_OPA_COVER); YMGUI_Draw_Fill(s, &b, c, GY_OPA_COVER);
	YMGUI_Draw_Fill(s, &l, c, GY_OPA_COVER); YMGUI_Draw_Fill(s, &r, c, GY_OPA_COVER);
	GYcoord sy = abs->y + abs->h - FD_ACTION_H - FD_NAME_H - FD_STATUS_H - FD_PAD;
	YMGUI_Draw_Text(s, &YMGUI_Font_Default, abs->x + FD_PAD, sy, d->status,
		GY_ARGB(0xFF, 0xA8, 0xB0, 0xBC));
}

static GYTREENODE addTreeNode(GYfd_data* d, GYTREENODE parent, const char* name, uint8 is_dir)
{
	if (d->node_count >= d->entry_cap) return NULL;
	GYfd_node_meta* m = &d->node_meta[d->node_count];
	m->name = d->node_names + (d->name_cap + 1) * d->node_count;
	copyText(m->name, d->name_cap + 1, name);
	GYTREENODE node = YMGUI_TreeView_AddNode(d->tree, parent, name, is_dir);
	if (!node) return NULL;
	YMGUI_TreeView_SetNodeUserPtr(node, m);
	d->node_count++;
	return node;
}

static void treeExpand(GYOBJ tree, GYTREENODE node)
{
	GYOBJ fd = tree && tree->parent ? tree->parent->parent : NULL;
	GYfd_data* d = fd ? (GYfd_data*)fd->user_data : NULL;
	if (!d || !buildNodePath(d, node, d->joined)) { if (fd) setStatus(fd, "Path is too long"); return; }
	void* dir = d->fs.open_dir ? d->fs.open_dir(d->fs_user, d->joined) : NULL;
	if (!dir) { setStatus(fd, "Cannot open directory"); return; }
	int rc = 0;
	while (d->node_count < d->entry_cap)
	{
		GYfiledialog_entry e = {d->scratch_name, d->name_cap + 1, 0, 0};
		d->scratch_name[0] = '\0';
		rc = d->fs.read_dir(d->fs_user, dir, &e);
		if (rc <= 0) break;
		d->scratch_name[d->name_cap] = '\0';
		if (!d->scratch_name[0] || strcmp(d->scratch_name, ".") == 0 || strcmp(d->scratch_name, "..") == 0) continue;
		if (!entryVisible(fd, d->joined, &e)) continue;
		if (!addTreeNode(d, node, d->scratch_name, e.is_dir)) { rc = -1; break; }
	}
	d->fs.close_dir(d->fs_user, dir);
	if (rc < 0) setStatus(fd, "Directory read failed");
	else if (d->node_count == d->entry_cap) setStatus(fd, "Entry limit reached");
	else setStatus(fd, "");
}

static int32 rootIndexOf(GYfd_data* d, GYTREENODE node)
{
	for (uint16 i = 0; i < d->entry_count; i++) if (d->root_nodes[i] == node) return i;
	return -1;
}

static void treeSelect(GYOBJ tree, GYTREENODE node)
{
	GYOBJ fd = tree && tree->parent ? tree->parent->parent : NULL;
	GYfd_data* d = fd ? (GYfd_data*)fd->user_data : NULL;
	if (!d) return;
	d->selected_node = node; d->selected = rootIndexOf(d, node);
	if (!YMGUI_TreeView_NodeIsDir(node)) YMGUI_TextInput_SetText(d->name_input, fullNodeName(node));
}

static void treeActivate(GYOBJ tree, GYTREENODE node)
{
	treeSelect(tree, node);
	GYOBJ fd = tree && tree->parent ? tree->parent->parent : NULL;
	if (fd) YMGUI_FileDialog_Confirm(fd);
}

static void clearFocusInDialog(GYOBJ fd)
{
	if (!fd || !fd->ctx || !fd->ctx->focus_obj) return;
	GYOBJ p = fd->ctx->focus_obj;
	while (p && p != fd) p = p->parent;
	if (p == fd) YMGUI_SetFocus(fd->ctx, NULL);
}

static void emitResult(GYOBJ fd, uint8 accepted, const char* path)
{
	GYfd_data* d = (GYfd_data*)fd->user_data;
	clearFocusInDialog(fd); YMGUI_Obj_SetHidden(fd, 1);
	if (d->result_cb) d->result_cb(fd, accepted, path, d->cb_user);
}

uint8 YMGUI_FileDialog_Refresh(GYOBJ fd)
{
	if (!fd || !fd->user_data) return 0;
	GYfd_data* d = (GYfd_data*)fd->user_data;
	if (!d->fs.open_dir || !d->fs.read_dir || !d->fs.close_dir)
		{ setStatus(fd, "Filesystem callbacks not set"); return 0; }
	void* dir = d->fs.open_dir(d->fs_user, d->path);
	if (!dir) { setStatus(fd, "Cannot open directory"); return 0; }
	d->entry_count = 0;
	int rc = 0;
	while (d->entry_count < d->entry_cap)
	{
		GYfiledialog_entry* e = &d->entries[d->entry_count];
		e->name = d->names + (d->name_cap + 1) * d->entry_count;
		e->name_cap = d->name_cap + 1; e->name[0] = '\0'; e->is_dir = 0; e->size = 0;
		rc = d->fs.read_dir(d->fs_user, dir, e);
		if (rc <= 0) break;
		e->name[d->name_cap] = '\0';
		if (!e->name[0] || strcmp(e->name, ".") == 0 || strcmp(e->name, "..") == 0) continue;
		if (!entryVisible(fd, d->path, e)) continue;
		d->entry_count++;
	}
	d->fs.close_dir(d->fs_user, dir);
	if (rc < 0) { setStatus(fd, "Directory read failed"); return 0; }
	YMGUI_TreeView_Clear(d->tree);
	d->node_count = 0; d->selected = -1; d->selected_node = NULL;
	for (uint16 i = 0; i < d->entry_count; i++) d->root_nodes[i] = NULL;
	for (uint8 pass = 0; pass < 2; pass++)
		for (uint16 i = 0; i < d->entry_count; i++)
			if ((pass == 0) == (d->entries[i].is_dir != 0))
			{
				d->root_nodes[i] = addTreeNode(d, NULL, d->entries[i].name, d->entries[i].is_dir);
				if (!d->root_nodes[i]) { setStatus(fd, "Cannot add tree node"); return 0; }
			}
	if (d->entry_count == d->entry_cap) setStatus(fd, "Entry limit reached"); else setStatus(fd, "");
	return 1;
}

uint8 YMGUI_FileDialog_Navigate(GYOBJ fd, const char* path)
{
	if (!fd || !fd->user_data || !path) return 0;
	GYfd_data* d = (GYfd_data*)fd->user_data;
	if (strlen(path) > d->path_cap) { setStatus(fd, "Path is too long"); return 0; }
	copyText(d->old_path, d->path_cap + 1, d->path);
	copyText(d->joined, d->path_cap + 1, path); // path 可能正是 path_input 内部缓冲
	copyText(d->path, d->path_cap + 1, d->joined);
	if (!YMGUI_FileDialog_Refresh(fd)) { copyText(d->path, d->path_cap + 1, d->old_path); return 0; }
	YMGUI_TextInput_SetText(d->path_input, d->path);
	YMGUI_TextInput_SetText(d->name_input, "");
	setStatus(fd, "Path loaded");
	return 1;
}

uint8 YMGUI_FileDialog_Up(GYOBJ fd)
{
	if (!fd || !fd->user_data) return 0;
	GYfd_data* d = (GYfd_data*)fd->user_data;
	copyText(d->joined, d->path_cap + 1, d->path); parentPath(d->joined);
	return YMGUI_FileDialog_Navigate(fd, d->joined);
}

uint8 YMGUI_FileDialog_Go(GYOBJ fd)
{
	if (!fd || !fd->user_data) return 0;
	GYfd_data* d = (GYfd_data*)fd->user_data;
	if (d->selected_node && YMGUI_TreeView_NodeIsDir(d->selected_node))
	{
		if (!buildNodePath(d, d->selected_node, d->joined))
			{ setStatus(fd, "Path is too long"); return 0; }
		return YMGUI_FileDialog_Navigate(fd, d->joined);
	}
	return YMGUI_FileDialog_Navigate(fd, YMGUI_TextInput_GetText(d->path_input));
}

uint8 YMGUI_FileDialog_SelectIndex(GYOBJ fd, uint16 index)
{
	if (!fd || !fd->user_data) return 0;
	GYfd_data* d = (GYfd_data*)fd->user_data;
	if (index >= d->entry_count || !d->root_nodes[index]) return 0;
	d->selected = index; d->selected_node = d->root_nodes[index];
	YMGUI_TreeView_SetSelectedNode(d->tree, d->selected_node);
	if (!d->entries[index].is_dir) YMGUI_TextInput_SetText(d->name_input, d->entries[index].name);
	return 1;
}

uint8 YMGUI_FileDialog_ActivateIndex(GYOBJ fd, uint16 index)
{
	if (!YMGUI_FileDialog_SelectIndex(fd, index)) return 0;
	GYfd_data* d = (GYfd_data*)fd->user_data;
	if (d->entries[index].is_dir)
	{
		if (!joinPath(d, d->path, d->entries[index].name)) { setStatus(fd, "Path is too long"); return 0; }
		return YMGUI_FileDialog_Navigate(fd, d->joined);
	}
	if (d->mode == GY_FILE_DIALOG_OPEN_FILE) return YMGUI_FileDialog_Confirm(fd);
	return 1;
}

uint8 YMGUI_FileDialog_Confirm(GYOBJ fd)
{
	if (!fd || !fd->user_data) return 0;
	GYfd_data* d = (GYfd_data*)fd->user_data;
	if (d->mode == GY_FILE_DIALOG_SELECT_DIRECTORY)
	{
		if (d->selected_node && YMGUI_TreeView_NodeIsDir(d->selected_node))
		{
			if (!buildNodePath(d, d->selected_node, d->joined)) { setStatus(fd, "Path is too long"); return 0; }
			emitResult(fd, 1, d->joined);
		}
		else emitResult(fd, 1, d->path);
		return 1;
	}
	const char* name = YMGUI_TextInput_GetText(d->name_input);
	if (!validName(name)) { setStatus(fd, "Choose a valid file name"); return 0; }
	if (d->selected_node && !YMGUI_TreeView_NodeIsDir(d->selected_node) && strcmp(name, fullNodeName(d->selected_node)) == 0)
	{
		if (!buildNodePath(d, d->selected_node, d->joined)) { setStatus(fd, "Path is too long"); return 0; }
	}
	else if (!joinPath(d, d->path, name)) { setStatus(fd, "Path is too long"); return 0; }
	uint8 exists = 0, is_dir = 0;
	if (!d->fs.stat_path || !d->fs.stat_path(d->fs_user, d->joined, &exists, &is_dir))
		{ setStatus(fd, "Cannot inspect target"); return 0; }
	if (d->mode == GY_FILE_DIALOG_OPEN_FILE && (!exists || is_dir))
		{ setStatus(fd, "Select an existing file"); return 0; }
	if (d->mode == GY_FILE_DIALOG_SAVE_FILE && exists)
	{
		if (is_dir) { setStatus(fd, "Target is a directory"); return 0; }
		if (!d->overwrite_cb || !d->overwrite_cb(fd, d->joined, d->cb_user))
			{ setStatus(fd, "Overwrite not confirmed"); return 0; }
	}
	emitResult(fd, 1, d->joined); return 1;
}

uint8 YMGUI_FileDialog_NewDirectory(GYOBJ fd, const char* name)
{
	if (!fd || !fd->user_data) return 0;
	GYfd_data* d = (GYfd_data*)fd->user_data;
	if (!validName(name)) { setStatus(fd, "Enter a valid folder name"); return 0; }
	if (!joinPath(d, d->path, name)) { setStatus(fd, "Path is too long"); return 0; }
	if (!d->fs.make_dir || !d->fs.make_dir(d->fs_user, d->joined))
		{ setStatus(fd, "Cannot create folder"); return 0; }
	YMGUI_TextInput_SetText(d->name_input, ""); return YMGUI_FileDialog_Refresh(fd);
}

static GYOBJ fdFromButton(GYOBJ b) { return b && b->parent ? b->parent->parent : NULL; }
static void upClick(GYOBJ b) { YMGUI_FileDialog_Up(fdFromButton(b)); }
static void goClick(GYOBJ b)
{ YMGUI_FileDialog_Go(fdFromButton(b)); }
static void refreshClick(GYOBJ b)
{
	GYOBJ fd = fdFromButton(b);
	if (fd && YMGUI_FileDialog_Refresh(fd)) setStatus(fd, "Refreshed");
}
static void mkdirClick(GYOBJ b)
{ GYOBJ fd = fdFromButton(b); GYfd_data* d = fd ? (GYfd_data*)fd->user_data : NULL; if (d) YMGUI_FileDialog_NewDirectory(fd, YMGUI_TextInput_GetText(d->name_input)); }
static void okClick(GYOBJ b) { YMGUI_FileDialog_Confirm(fdFromButton(b)); }
static void cancelClick(GYOBJ b) { GYOBJ fd = fdFromButton(b); if (fd) emitResult(fd, 0, NULL); }
static void pathSubmitted(GYOBJ input, const char* path)
{
	GYOBJ card = input ? input->parent : NULL;
	GYOBJ fd = card ? card->parent : NULL;
	if (fd) YMGUI_FileDialog_Navigate(fd, path);
}

static void fdFree(GYOBJ obj)
{
	GYfd_data* d = (GYfd_data*)obj->user_data;
	if (!d) return;
	if (d->root_nodes) GY_free0(d->root_nodes);
	if (d->node_meta) GY_free0(d->node_meta);
	if (d->entries) GY_free0(d->entries);
	if (d->node_names) GY_free0(d->node_names);
	if (d->names) GY_free0(d->names);
	if (d->scratch_name) GY_free0(d->scratch_name);
	if (d->path) GY_free0(d->path);
	if (d->joined) GY_free0(d->joined);
	if (d->old_path) GY_free0(d->old_path);
	GY_free0(d); obj->user_data = NULL;
}

GYOBJ YMGUI_Creat_FileDialog_Creat(GYCTX ctx, GYcoord width, GYcoord height,
		size_t path_capacity, size_t name_capacity, uint16 entry_capacity)
{
	if (!ctx || !ctx->top_layer || width < 280 || height < 220 || width > ctx->top_layer->area.w || height > ctx->top_layer->area.h ||
		path_capacity == 0 || name_capacity == 0 || entry_capacity == 0 || entry_capacity > GY_FILEDIALOG_MAX_ENTRIES) return NULL;
	size_t sm = (size_t)-1;
	if (path_capacity == sm || name_capacity == sm || name_capacity + 1 > sm / entry_capacity ||
		sizeof(GYfiledialog_entry) > sm / entry_capacity || sizeof(GYfd_node_meta) > sm / entry_capacity || sizeof(GYTREENODE) > sm / entry_capacity) return NULL;
	GYOBJ fd = YMGUI_Creat_Obj_Creat(ctx->top_layer, 0, 0, ctx->top_layer->area.w, ctx->top_layer->area.h);
	if (!fd) return NULL;
	GYfd_data* d = (GYfd_data*)GY_malloc0(sizeof(GYfd_data));
	if (!d) { YMGUI_Free_ObjFree(fd); return NULL; }
	GY_memset(d, 0, sizeof(*d)); fd->user_data = d; fd->free_cb = fdFree;
	d->path_cap = path_capacity; d->name_cap = name_capacity; d->entry_cap = entry_capacity; d->selected = -1;
	d->path = GY_malloc0(path_capacity + 1); d->joined = GY_malloc0(path_capacity + 1); d->old_path = GY_malloc0(path_capacity + 1);
	d->scratch_name = GY_malloc0(name_capacity + 1); d->names = GY_malloc0((name_capacity + 1) * entry_capacity);
	d->node_names = GY_malloc0((name_capacity + 1) * entry_capacity); d->entries = GY_malloc0(sizeof(GYfiledialog_entry) * entry_capacity);
	d->node_meta = GY_malloc0(sizeof(GYfd_node_meta) * entry_capacity); d->root_nodes = GY_malloc0(sizeof(GYTREENODE) * entry_capacity);
	if (!d->path || !d->joined || !d->old_path || !d->scratch_name || !d->names || !d->node_names || !d->entries || !d->node_meta || !d->root_nodes)
		{ YMGUI_Free_ObjFree(fd); return NULL; }
	GY_memset(d->path, 0, path_capacity + 1); GY_memset(d->joined, 0, path_capacity + 1); GY_memset(d->old_path, 0, path_capacity + 1);
	GY_memset(d->scratch_name, 0, name_capacity + 1); GY_memset(d->names, 0, (name_capacity + 1) * entry_capacity);
	GY_memset(d->node_names, 0, (name_capacity + 1) * entry_capacity); GY_memset(d->entries, 0, sizeof(GYfiledialog_entry) * entry_capacity);
	GY_memset(d->node_meta, 0, sizeof(GYfd_node_meta) * entry_capacity); GY_memset(d->root_nodes, 0, sizeof(GYTREENODE) * entry_capacity);
	fd->draw_cb = backdropDraw; fd->event_cb = swallowEvent;
	GYcoord x = (fd->area.w - width) / 2, y = (fd->area.h - height) / 2;
	d->card = YMGUI_Creat_Obj_Creat(fd, x, y, width, height);
	if (!d->card) { YMGUI_Free_ObjFree(fd); return NULL; }
	d->card->draw_cb = cardDraw; d->card->event_cb = swallowEvent; d->card->state |= GY_STATE_ClipChildren;
	GYcoord tool_y = FD_PAD + FD_PATH_H + 5, tree_y = tool_y + FD_TOOL_H + 5;
	GYcoord tree_h = height - tree_y - FD_STATUS_H - FD_NAME_H - FD_ACTION_H - FD_PAD * 2;
	d->path_input = YMGUI_Creat_TextInput_Creat(d->card, FD_PAD, FD_PAD, width - FD_PAD * 2, FD_PATH_H, path_capacity);
	d->btn_up = YMGUI_Creat_Button_Creat(d->card, FD_PAD, tool_y, 54, FD_TOOL_H);
	d->btn_go = YMGUI_Creat_Button_Creat(d->card, FD_PAD + 60, tool_y, 54, FD_TOOL_H);
	d->btn_refresh = YMGUI_Creat_Button_Creat(d->card, FD_PAD + 120, tool_y, 70, FD_TOOL_H);
	d->btn_mkdir = YMGUI_Creat_Button_Creat(d->card, FD_PAD + 196, tool_y, 68, FD_TOOL_H);
	d->tree = YMGUI_Creat_TreeView_Creat(d->card, FD_PAD, tree_y, width - FD_PAD * 2, tree_h);
	GYcoord name_y = height - FD_ACTION_H - FD_NAME_H - FD_PAD;
	d->name_input = YMGUI_Creat_TextInput_Creat(d->card, FD_PAD, name_y, width - FD_PAD * 2, FD_NAME_H, name_capacity);
	d->btn_ok = YMGUI_Creat_Button_Creat(d->card, width - 164, height - FD_ACTION_H - 2, 76, FD_ACTION_H - 2);
	d->btn_cancel = YMGUI_Creat_Button_Creat(d->card, width - 82, height - FD_ACTION_H - 2, 74, FD_ACTION_H - 2);
	if (!d->path_input || !d->btn_up || !d->btn_go || !d->btn_refresh || !d->btn_mkdir || !d->tree || !d->name_input || !d->btn_ok || !d->btn_cancel)
		{ YMGUI_Free_ObjFree(fd); return NULL; }
	YMGUI_Button_SetText(d->btn_up, "Up"); YMGUI_Button_SetClicked(d->btn_up, upClick);
	YMGUI_Button_SetText(d->btn_go, "Go"); YMGUI_Button_SetClicked(d->btn_go, goClick);
	YMGUI_Button_SetText(d->btn_refresh, "Refresh"); YMGUI_Button_SetClicked(d->btn_refresh, refreshClick);
	YMGUI_Button_SetText(d->btn_mkdir, "New dir"); YMGUI_Button_SetClicked(d->btn_mkdir, mkdirClick);
	YMGUI_Button_SetText(d->btn_ok, "Open"); YMGUI_Button_SetClicked(d->btn_ok, okClick);
	YMGUI_Button_SetText(d->btn_cancel, "Cancel"); YMGUI_Button_SetClicked(d->btn_cancel, cancelClick);
	YMGUI_TreeView_SetExpandCb(d->tree, treeExpand); YMGUI_TreeView_SetSelectCb(d->tree, treeSelect); YMGUI_TreeView_SetActivateCb(d->tree, treeActivate);
	YMGUI_TextInput_SetSubmitted(d->path_input, pathSubmitted);
	YMGUI_Obj_SetHidden(fd, 1); return fd;
}

void YMGUI_FileDialog_SetFS(GYOBJ fd, const GYfiledialog_fs* fs, void* user)
{ if (fd && fd->user_data) { GYfd_data* d = fd->user_data; GY_memset(&d->fs, 0, sizeof(d->fs)); if (fs) d->fs = *fs; d->fs_user = user; } }
void YMGUI_FileDialog_SetResultCb(GYOBJ fd, GYfiledialog_result_cb cb, void* user)
{ if (fd && fd->user_data) { GYfd_data* d = fd->user_data; d->result_cb = cb; d->cb_user = user; } }
void YMGUI_FileDialog_SetOverwriteCb(GYOBJ fd, GYfiledialog_overwrite_cb cb)
{ if (fd && fd->user_data) ((GYfd_data*)fd->user_data)->overwrite_cb = cb; }
void YMGUI_FileDialog_SetFilterCb(GYOBJ fd, GYfiledialog_filter_cb cb, void* user)
{ if (fd && fd->user_data) { GYfd_data* d = fd->user_data; d->filter_cb = cb; d->filter_user = user; } }

uint8 YMGUI_FileDialog_Show(GYOBJ fd, GYfiledialog_mode mode, const char* path, const char* name)
{
	if (!fd || !fd->user_data || mode > GY_FILE_DIALOG_SELECT_DIRECTORY || !path) return 0;
	GYfd_data* d = fd->user_data;
	if (strlen(path) > d->path_cap || (name && strlen(name) > d->name_cap)) return 0;
	d->mode = mode;
	copyText(d->old_path, d->path_cap + 1, d->path); copyText(d->path, d->path_cap + 1, path);
	if (!YMGUI_FileDialog_Refresh(fd)) { copyText(d->path, d->path_cap + 1, d->old_path); return 0; }
	YMGUI_TextInput_SetText(d->path_input, d->path); YMGUI_TextInput_SetText(d->name_input, name ? name : "");
	YMGUI_Button_SetText(d->btn_ok, mode == GY_FILE_DIALOG_OPEN_FILE ? "Open" : mode == GY_FILE_DIALOG_SAVE_FILE ? "Save" : "Select");
	setStatus(fd, ""); YMGUI_Obj_SetHidden(fd, 0); YMGUI_SetFocus(fd->ctx, d->path_input); return 1;
}

void YMGUI_FileDialog_Close(GYOBJ fd) { if (fd) { clearFocusInDialog(fd); YMGUI_Obj_SetHidden(fd, 1); } }
uint8 YMGUI_FileDialog_IsShown(GYOBJ fd) { return fd && !(fd->state & GY_STATE_Hidden); }
GYfiledialog_mode YMGUI_FileDialog_GetMode(GYOBJ fd) { return fd && fd->user_data ? ((GYfd_data*)fd->user_data)->mode : GY_FILE_DIALOG_OPEN_FILE; }
const char* YMGUI_FileDialog_GetPath(GYOBJ fd) { return fd && fd->user_data ? ((GYfd_data*)fd->user_data)->path : ""; }
const char* YMGUI_FileDialog_GetName(GYOBJ fd) { GYfd_data* d = fd && fd->user_data ? fd->user_data : NULL; return d ? YMGUI_TextInput_GetText(d->name_input) : ""; }
uint16 YMGUI_FileDialog_GetEntryCount(GYOBJ fd) { return fd && fd->user_data ? ((GYfd_data*)fd->user_data)->entry_count : 0; }
const GYfiledialog_entry* YMGUI_FileDialog_GetEntry(GYOBJ fd, uint16 i) { GYfd_data* d = fd && fd->user_data ? fd->user_data : NULL; return d && i < d->entry_count ? &d->entries[i] : NULL; }
int32 YMGUI_FileDialog_GetSelectedIndex(GYOBJ fd) { return fd && fd->user_data ? ((GYfd_data*)fd->user_data)->selected : -1; }
const char* YMGUI_FileDialog_GetStatus(GYOBJ fd) { return fd && fd->user_data ? ((GYfd_data*)fd->user_data)->status : ""; }

#endif
