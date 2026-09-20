#ifndef YMGUI_FILEDIALOG_H
#define YMGUI_FILEDIALOG_H

#include "YMGUI_PubType.h"
#include "YMGUI_PubDefine.h"
#include "YMGUI_Obj.h"

#if YMGUI_FILEDIALOG

#ifndef GY_FILEDIALOG_MAX_ENTRIES
#define GY_FILEDIALOG_MAX_ENTRIES 1024 // 默认 GYcoord=int16 时保证行内容高度不溢出
#endif

typedef enum
{
	GY_FILE_DIALOG_OPEN_FILE = 0,
	GY_FILE_DIALOG_SAVE_FILE,
	GY_FILE_DIALOG_SELECT_DIRECTORY
} GYfiledialog_mode;

typedef struct
{
	char* name;       // read_dir 填入,容量为创建时的 name_capacity+1
	size_t name_cap;  // 可写字节数,包含结尾 '\0'
	uint8 is_dir;
	uint32 size;
} GYfiledialog_entry;

// open_dir 成功返回枚举句柄,NULL 表示失败;read_dir:1=一项,0=结束,-1=错误。
typedef struct
{
	void* (*open_dir)(void* user, const char* path);
	int   (*read_dir)(void* user, void* dir, GYfiledialog_entry* entry);
	void  (*close_dir)(void* user, void* dir);
	// stat 成功返回 1 并填写 exists/is_dir;查询失败返回 0。
	uint8 (*stat_path)(void* user, const char* path, uint8* exists, uint8* is_dir);
	uint8 (*make_dir)(void* user, const char* path);
} GYfiledialog_fs;

typedef void  (*GYfiledialog_result_cb)(GYOBJ dialog, uint8 accepted,
		const char* path, void* user);
typedef uint8 (*GYfiledialog_overwrite_cb)(GYOBJ dialog, const char* path,
		void* user);
// 仅控制条目是否显示，不参与手动输入路径或最终确认校验。返回 1=显示，0=隐藏。
typedef uint8 (*GYfiledialog_filter_cb)(GYOBJ dialog, GYfiledialog_mode mode,
		const char* parent_path, const GYfiledialog_entry* entry, void* user);
// 可选文案翻译；key 为控件默认英文，返回 NULL 时保留原文。
// 返回字符串至少在当前调用期间有效；user 由调用方管理，文件名/路径不翻译。
typedef const char* (*GYfiledialog_translate_cb)(const char* key, void* user);

// 三个容量均不含结尾 '\0';创建时一次性分配,运行中不随目录内容扩容。
GYOBJ YMGUI_Creat_FileDialog_Creat(GYCTX ctx, GYcoord width, GYcoord height,
		size_t path_capacity, size_t name_capacity, uint16 entry_capacity);
void YMGUI_FileDialog_SetFS(GYOBJ dialog, const GYfiledialog_fs* fs, void* fs_user);
void YMGUI_FileDialog_SetResultCb(GYOBJ dialog, GYfiledialog_result_cb cb, void* user);
void YMGUI_FileDialog_SetOverwriteCb(GYOBJ dialog, GYfiledialog_overwrite_cb cb);
void YMGUI_FileDialog_SetFilterCb(GYOBJ dialog, GYfiledialog_filter_cb cb, void* user);
void YMGUI_FileDialog_SetTranslator(GYOBJ dialog, GYfiledialog_translate_cb cb, void* user);

uint8 YMGUI_FileDialog_Show(GYOBJ dialog, GYfiledialog_mode mode,
		const char* initial_path, const char* initial_name);
void  YMGUI_FileDialog_Close(GYOBJ dialog);
uint8 YMGUI_FileDialog_IsShown(GYOBJ dialog);
uint8 YMGUI_FileDialog_Refresh(GYOBJ dialog);
uint8 YMGUI_FileDialog_Navigate(GYOBJ dialog, const char* path);
uint8 YMGUI_FileDialog_Up(GYOBJ dialog);
//有选中目录时进入该目录；否则提交顶部可编辑路径栏
uint8 YMGUI_FileDialog_Go(GYOBJ dialog);
uint8 YMGUI_FileDialog_SelectIndex(GYOBJ dialog, uint16 index);
uint8 YMGUI_FileDialog_ActivateIndex(GYOBJ dialog, uint16 index);
uint8 YMGUI_FileDialog_Confirm(GYOBJ dialog);
uint8 YMGUI_FileDialog_NewDirectory(GYOBJ dialog, const char* name);

GYfiledialog_mode YMGUI_FileDialog_GetMode(GYOBJ dialog);
const char* YMGUI_FileDialog_GetPath(GYOBJ dialog);
const char* YMGUI_FileDialog_GetName(GYOBJ dialog);
uint16 YMGUI_FileDialog_GetEntryCount(GYOBJ dialog);
const GYfiledialog_entry* YMGUI_FileDialog_GetEntry(GYOBJ dialog, uint16 index);
int32 YMGUI_FileDialog_GetSelectedIndex(GYOBJ dialog);
const char* YMGUI_FileDialog_GetStatus(GYOBJ dialog);

#endif
#endif
