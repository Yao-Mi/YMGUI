#ifndef MP_LRC_H
#define MP_LRC_H

#include "YMGUI_PubType.h"

//===========================================================================
// mp_lrc —— .lrc 歌词解析(app 侧,库不认识歌词)。
//   解析 [mm:ss.xx] 时间标签行,存成按时间排序的 {ms, text} 数组。
//   一行可带多个时间标签(共享同一文本);无标签行忽略。
//   提供"给定播放毫秒 → 当前应高亮的歌词行索引"查询。
//===========================================================================

#define MP_LRC_MAX_LINES 512
#define MP_LRC_TEXT_MAX  96 //每行文本上限(含 '\0'),与 Roller 一致

typedef struct
{
	int32 ms;                     //该行起始时间(毫秒)
	char  text[MP_LRC_TEXT_MAX];  //歌词文本
}MPlrcLine;

typedef struct
{
	MPlrcLine lines[MP_LRC_MAX_LINES];
	int32     count;
}MPlrc;

//清空
void  mp_lrc_clear(MPlrc* lr);
//从内存字符串解析(可多次调用累加前先 clear)。返回解析到的行数
int32 mp_lrc_parse(MPlrc* lr, const char* text);
//从文件解析(读不到返回 0,不改变 lr)。返回行数
int32 mp_lrc_load_file(MPlrc* lr, const char* path);
//手动加一行(深拷,自动按时间插入保持有序)。返回是否成功
uint8 mp_lrc_add(MPlrc* lr, int32 ms, const char* text);
//给定播放毫秒,返回当前应高亮行索引(最后一个 ms<=play_ms 的行);都在未来返回 0,空返回 -1
int32 mp_lrc_index_at(const MPlrc* lr, int32 play_ms);

#endif // !MP_LRC_H
