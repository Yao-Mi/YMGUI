#include "mp_lrc.h"
#include <stdio.h>
#include <string.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    mp_lrc.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-04
  *	@Description: .lrc 歌词解析(app 侧)。解析 [mm:ss.xx] 时间标签,存成按时间排序的行数组;支持
  *	              一行多标签;提供"播放毫秒 → 高亮行索引"查询。纯整数,无浮点。
  *	@Version:     1.0
  ***************************************************************************************************************************/

void mp_lrc_clear(MPlrc* lr)
{
	if (lr == NULL) return;
	lr->count = 0;
}

//深拷文本到定长槽(截断)
static void copyText(char* dst, const char* src, int32 len)
{
	int32 k = 0;
	while (k < len && k < MP_LRC_TEXT_MAX - 1 && src[k] != '\0'
	       && src[k] != '\r' && src[k] != '\n')
	{
		dst[k] = src[k];
		k++;
	}
	dst[k] = '\0';
}

uint8 mp_lrc_add(MPlrc* lr, int32 ms, const char* text)
{
	if (lr == NULL || lr->count >= MP_LRC_MAX_LINES) return 0;
	//找插入位(保持 ms 升序,相等则追加其后)
	int32 pos = lr->count;
	while (pos > 0 && lr->lines[pos - 1].ms > ms)
	{
		lr->lines[pos] = lr->lines[pos - 1];
		pos--;
	}
	lr->lines[pos].ms = ms;
	copyText(lr->lines[pos].text, (text != NULL) ? text : "", MP_LRC_TEXT_MAX - 1);
	lr->count++;
	return 1;
}

//解析一整行:抽出行首连续 [..] 时间标签,余下为文本。多标签共享文本
static void parseLine(MPlrc* lr, const char* line)
{
	//收集本行所有时间标签(毫秒)
	int32 tags[16];
	int32 ntag = 0;
	const char* p = line;
	while (*p == '[')
	{
		//形如 [mm:ss.xx] 或 [mm:ss]。非数字标签([ti:]/[ar:] 等)跳过
		const char* q = p + 1;
		int32 mm = 0, ss = 0, cs = 0;
		int32 got_digit = 0;
		//分钟
		while (*q >= '0' && *q <= '9') { mm = mm * 10 + (*q - '0'); q++; got_digit = 1; }
		if (!got_digit || *q != ':')
		{
			//非时间标签:跳到 ']' 之后
			while (*q != '\0' && *q != ']') q++;
			if (*q == ']') q++;
			p = q;
			continue;
		}
		q++; //跳 ':'
		while (*q >= '0' && *q <= '9') { ss = ss * 10 + (*q - '0'); q++; }
		if (*q == '.' || *q == ':')
		{
			q++;
			int32 digits = 0;
			while (*q >= '0' && *q <= '9' && digits < 3) { cs = cs * 10 + (*q - '0'); q++; digits++; }
			//归一到毫秒:两位=厘秒*10,三位=毫秒
			if (digits == 1) cs *= 100;
			else if (digits == 2) cs *= 10;
			//digits==3 已是毫秒
		}
		while (*q != '\0' && *q != ']') q++;
		if (*q == ']') q++;
		if (ntag < 16)
			tags[ntag++] = (mm * 60 + ss) * 1000 + cs;
		p = q;
	}
	if (ntag == 0)
		return; //无时间标签,忽略
	//余下 p.. 是文本(可空)
	int32 i;
	for (i = 0; i < ntag; i++)
		mp_lrc_add(lr, tags[i], p);
}

int32 mp_lrc_parse(MPlrc* lr, const char* text)
{
	if (lr == NULL || text == NULL) return 0;
	const char* p = text;
	char buf[256];
	while (*p != '\0')
	{
		int32 k = 0;
		while (*p != '\0' && *p != '\n' && k < (int32)sizeof(buf) - 1)
		{
			buf[k++] = *p++;
		}
		buf[k] = '\0';
		if (*p == '\n') p++;
		parseLine(lr, buf);
	}
	return lr->count;
}

int32 mp_lrc_load_file(MPlrc* lr, const char* path)
{
	if (lr == NULL || path == NULL) return 0;
	FILE* f = fopen(path, "rb");
	if (f == NULL) return 0;
	//整文件读入(歌词很小)
	static char buf[64 * 1024];
	size_t n = fread(buf, 1, sizeof(buf) - 1, f);
	fclose(f);
	buf[n] = '\0';
	mp_lrc_clear(lr);
	return mp_lrc_parse(lr, buf);
}

int32 mp_lrc_index_at(const MPlrc* lr, int32 play_ms)
{
	if (lr == NULL || lr->count <= 0) return -1;
	//最后一个 ms <= play_ms 的行(二分)
	int32 lo = 0, hi = lr->count - 1, ans = 0;
	if (play_ms < lr->lines[0].ms) return 0;
	while (lo <= hi)
	{
		int32 mid = (lo + hi) / 2;
		if (lr->lines[mid].ms <= play_ms) { ans = mid; lo = mid + 1; }
		else hi = mid - 1;
	}
	return ans;
}
