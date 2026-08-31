#include "YMGUI_PubDefine.h"
#include "YMGUI_Debug.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Event.h"
#include "YMGUI_Label.h"
#include "YMGUI_Button.h"
#include "YMGUI_TextInput.h"
#include "YMGUI_EditView.h"
#include "YMGUI_Font.h"
#include "YMGUI_DrawFill.h"
#include "YMGUI_Geom.h"
#include "SDL_LCD.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCR_W 640
#define SCR_H 480
#define BAND_H 60
#define PAGE_MAX_CANDIDATES 9
#define IME_QUERY_CAP 32
#define IME_WORD_CAP 96
#define IME_COMPOSITE_CAP 12
#define IME_COMPOSE_BEAM 6
#define SYMBOLS_PER_PAGE 36
#define SYMBOL_CATEGORY_COUNT 7
#define SYMBOL_CATEGORY_CAP 108

#ifndef GB2312_BIN_PATH
#define GB2312_BIN_PATH "gb2312_glyphs.bin"
#endif
#ifndef PHRASE_BIN_PATH
#define PHRASE_BIN_PATH "phrases_rime.bin"
#endif
#ifndef CHAR_BIN_PATH
#define CHAR_BIN_PATH "pinyin_gb2312.bin"
#endif
#ifndef ENGLISH_BIN_PATH
#define ENGLISH_BIN_PATH "english_words.bin"
#endif
extern const uint16 YMGUI_GB2312_cps[];
extern const uint16 YMGUI_GB2312_glyph_count;
static FILE* s_blob;
static uint32 flashRead(const GYfont* font, uint32 off, uint32 len, uint8* buf)
{
	(void)font;
	if (!s_blob || fseek(s_blob, (long)off, SEEK_SET) != 0) return 0;
	return (uint32)fread(buf, 1, len, s_blob);
}
static GYfont s_gb_font = { NULL, YMGUI_GB2312_cps, 0, 0, 0, 16, 16, 8, 4, NULL, flashRead };

typedef struct { const char* py; const char* initials; const char* word; uint32 frequency; } PhraseEntry;
typedef struct { const char* py; const char* ch; uint32 frequency; } ImeCharEntry;
typedef struct { const char* word; uint32 frequency; } EnglishEntry;
#if defined(YMGUI_IME_PHRASES_EXTERNAL)
static PhraseEntry* s_phrases;
static int s_phrase_count;
static uint8* s_phrase_file_data;
static uint32 s_phrase_buckets[27];
#define S_PHRASE_COUNT s_phrase_count
#elif defined(YMGUI_IME_PHRASES_OFF)
static const PhraseEntry* s_phrases;
static const uint32 s_phrase_buckets[27] = { 0 };
#define S_PHRASE_COUNT 0
#else
#include "phrases_rime.inc"
#endif

#if defined(YMGUI_IME_CHARS_EXTERNAL)
static ImeCharEntry* s_full_dict;
static int s_char_count;
static uint8* s_char_file_data;
static uint32 s_char_buckets[27];
#define S_CHAR_COUNT s_char_count
#elif defined(YMGUI_IME_CHARS_OFF)
static const ImeCharEntry* s_full_dict;
static const uint32 s_char_buckets[27] = { 0 };
#define S_CHAR_COUNT 0
#else
#include "pinyin_gb2312.inc"
#define S_CHAR_COUNT S_FULL_DICT_COUNT
#endif

#if defined(YMGUI_IME_ENGLISH_EXTERNAL)
static EnglishEntry* s_english_words;
static int s_english_count;
static uint8* s_english_file_data;
static uint32 s_english_buckets[27];
#define S_ENGLISH_COUNT s_english_count
#else
static const EnglishEntry* s_english_words;
static const uint32 s_english_buckets[27] = { 0 };
#define S_ENGLISH_COUNT 0
#endif

#if defined(YMGUI_IME_PHRASES_EXTERNAL) || defined(YMGUI_IME_CHARS_EXTERNAL) || defined(YMGUI_IME_ENGLISH_EXTERNAL)
static uint32 readLe32(const uint8* p)
{
	return (uint32)p[0] | ((uint32)p[1] << 8) | ((uint32)p[2] << 16) | ((uint32)p[3] << 24);
}
#endif

#if defined(YMGUI_IME_PHRASES_EXTERNAL)
static int loadPhraseDictionary(void)
{
	FILE* file = fopen(PHRASE_BIN_PATH, "rb");
	if (!file) { gy_log_print("IME phrase file not found; character-only mode\n"); return 0; }
	if (fseek(file, 0, SEEK_END) != 0) { fclose(file); return 0; }
	long file_size_long = ftell(file);
	if (file_size_long < 28 || fseek(file, 0, SEEK_SET) != 0) { fclose(file); return 0; }
	size_t file_size = (size_t)file_size_long;
	uint8* data = (uint8*)GY_malloc1(file_size);
	if (!data || fread(data, 1, file_size, file) != file_size) { if (data) GY_free1(data); fclose(file); return 0; }
	fclose(file);
	uint32 count = readLe32(data + 8), blob_size = readLe32(data + 12);
	uint32 header_size = readLe32(data + 16), record_size = readLe32(data + 20), bucket_count = readLe32(data + 24);
	size_t records_size = (size_t)count * record_size;
	if (memcmp(data, "YMIMEP2\0", 8) != 0 || count > 1000000u || bucket_count != 26u || record_size != 16u ||
	    header_size != 28u + (bucket_count + 1u) * 4u || header_size > file_size ||
	    records_size / record_size != count || records_size > file_size - header_size ||
	    blob_size != file_size - header_size - records_size) {
		GY_free1(data); gy_log_print("IME phrase file invalid; character-only mode\n"); return 0;
	}
	for (uint32 i = 0; i <= bucket_count; ++i) s_phrase_buckets[i] = readLe32(data + 28u + i * 4u);
	if (s_phrase_buckets[0] != 0u || s_phrase_buckets[bucket_count] != count) {
		GY_free1(data); gy_log_print("IME phrase file invalid; character-only mode\n"); return 0;
	}
	for (uint32 i = 0; i < bucket_count; ++i) if (s_phrase_buckets[i] > s_phrase_buckets[i + 1]) {
		GY_free1(data); gy_log_print("IME phrase file invalid; character-only mode\n"); return 0;
	}
	PhraseEntry* entries = (PhraseEntry*)GY_malloc1((size_t)count * sizeof(PhraseEntry));
	if (!entries) { GY_free1(data); return 0; }
	uint8* blob = data + header_size + records_size;
	for (uint32 i = 0; i < count; ++i) {
		const uint8* record = data + header_size + (size_t)i * record_size;
		uint32 offsets[3] = { readLe32(record), readLe32(record + 4), readLe32(record + 8) };
		for (int j = 0; j < 3; ++j) if (offsets[j] >= blob_size || !memchr(blob + offsets[j], '\0', blob_size - offsets[j])) {
			GY_free1(entries); GY_free1(data); gy_log_print("IME phrase file invalid; character-only mode\n"); return 0;
		}
		entries[i].py = (const char*)blob + offsets[0]; entries[i].initials = (const char*)blob + offsets[1];
		entries[i].word = (const char*)blob + offsets[2]; entries[i].frequency = readLe32(record + 12);
		int bucket = entries[i].py[0] - 'a';
		if (bucket < 0 || bucket >= 26 || i < s_phrase_buckets[bucket] || i >= s_phrase_buckets[bucket + 1]) {
			GY_free1(entries); GY_free1(data); gy_log_print("IME phrase file invalid; character-only mode\n"); return 0;
		}
	}
	s_phrases = entries; s_phrase_count = (int)count; s_phrase_file_data = data;
	gy_log_print("IME external phrase dictionary loaded\n");
	return 1;
}
static void freePhraseDictionary(void)
{
	if (s_phrases) GY_free1(s_phrases);
	if (s_phrase_file_data) GY_free1(s_phrase_file_data);
	s_phrases = NULL; s_phrase_file_data = NULL; s_phrase_count = 0;
	memset(s_phrase_buckets, 0, sizeof(s_phrase_buckets));
}
#else
static int loadPhraseDictionary(void) { return S_PHRASE_COUNT > 0; }
static void freePhraseDictionary(void) { }
#endif

#if defined(YMGUI_IME_CHARS_EXTERNAL)
static int loadCharDictionary(void)
{
	FILE* file = fopen(CHAR_BIN_PATH, "rb");
	if (!file) { gy_log_print("IME character file not found; single-character candidates disabled\n"); return 0; }
	if (fseek(file, 0, SEEK_END) != 0) { fclose(file); return 0; }
	long file_size_long = ftell(file);
	if (file_size_long < 136 || fseek(file, 0, SEEK_SET) != 0) { fclose(file); return 0; }
	size_t file_size = (size_t)file_size_long;
	uint8* data = (uint8*)GY_malloc1(file_size);
	if (!data || fread(data, 1, file_size, file) != file_size) { if (data) GY_free1(data); fclose(file); return 0; }
	fclose(file);
	uint32 count = readLe32(data + 8), blob_size = readLe32(data + 12);
	uint32 header_size = readLe32(data + 16), record_size = readLe32(data + 20), bucket_count = readLe32(data + 24);
	size_t records_size = (size_t)count * record_size;
	if (memcmp(data, "YMIMEC1\0", 8) != 0 || count > 100000u || bucket_count != 26u || record_size != 12u ||
	    header_size != 136u || records_size / record_size != count || header_size > file_size ||
	    records_size > file_size - header_size || blob_size != file_size - header_size - records_size) {
		GY_free1(data); gy_log_print("IME character file invalid; single-character candidates disabled\n"); return 0;
	}
	for (uint32 i = 0; i <= bucket_count; ++i) s_char_buckets[i] = readLe32(data + 28u + i * 4u);
	if (s_char_buckets[0] != 0u || s_char_buckets[26] != count) { GY_free1(data); return 0; }
	for (uint32 i = 0; i < 26; ++i) if (s_char_buckets[i] > s_char_buckets[i + 1]) { GY_free1(data); return 0; }
	ImeCharEntry* entries = (ImeCharEntry*)GY_malloc1((size_t)count * sizeof(ImeCharEntry));
	if (!entries) { GY_free1(data); return 0; }
	uint8* blob = data + header_size + records_size;
	for (uint32 i = 0; i < count; ++i) {
		const uint8* record = data + header_size + (size_t)i * record_size;
		uint32 py_offset = readLe32(record), char_offset = readLe32(record + 4);
		if (py_offset >= blob_size || char_offset >= blob_size ||
		    !memchr(blob + py_offset, '\0', blob_size - py_offset) || !memchr(blob + char_offset, '\0', blob_size - char_offset)) {
			GY_free1(entries); GY_free1(data); gy_log_print("IME character file invalid; single-character candidates disabled\n"); return 0;
		}
		entries[i].py = (const char*)blob + py_offset; entries[i].ch = (const char*)blob + char_offset;
		entries[i].frequency = readLe32(record + 8);
		int bucket = entries[i].py[0] - 'a';
		if (bucket < 0 || bucket >= 26 || i < s_char_buckets[bucket] || i >= s_char_buckets[bucket + 1]) {
			GY_free1(entries); GY_free1(data); gy_log_print("IME character file invalid; single-character candidates disabled\n"); return 0;
		}
	}
	s_full_dict = entries; s_char_count = (int)count; s_char_file_data = data;
	gy_log_print("IME external character dictionary loaded\n");
	return 1;
}
static void freeCharDictionary(void)
{
	if (s_full_dict) GY_free1(s_full_dict);
	if (s_char_file_data) GY_free1(s_char_file_data);
	s_full_dict = NULL; s_char_file_data = NULL; s_char_count = 0;
	memset(s_char_buckets, 0, sizeof(s_char_buckets));
}
#else
static int loadCharDictionary(void) { return S_CHAR_COUNT > 0; }
static void freeCharDictionary(void) { }
#endif

#if defined(YMGUI_IME_ENGLISH_EXTERNAL)
static int loadEnglishDictionary(void)
{
	FILE* file = fopen(ENGLISH_BIN_PATH, "rb");
	if (!file) { gy_log_print("IME English file not found; direct English input mode\n"); return 0; }
	if (fseek(file, 0, SEEK_END) != 0) { fclose(file); return 0; }
	long file_size_long = ftell(file);
	if (file_size_long < 136 || fseek(file, 0, SEEK_SET) != 0) { fclose(file); return 0; }
	size_t file_size = (size_t)file_size_long;
	uint8* data = (uint8*)GY_malloc1(file_size);
	if (!data || fread(data, 1, file_size, file) != file_size) { if (data) GY_free1(data); fclose(file); return 0; }
	fclose(file);
	uint32 count = readLe32(data + 8), blob_size = readLe32(data + 12);
	uint32 header_size = readLe32(data + 16), record_size = readLe32(data + 20), bucket_count = readLe32(data + 24);
	size_t records_size = (size_t)count * record_size;
	if (memcmp(data, "YMIMEE1\0", 8) != 0 || count > 1000000u || bucket_count != 26u || record_size != 8u ||
	    header_size != 136u || records_size / record_size != count || header_size > file_size ||
	    records_size > file_size - header_size || blob_size != file_size - header_size - records_size) {
		GY_free1(data); gy_log_print("IME English file invalid; direct English input mode\n"); return 0;
	}
	for (uint32 i = 0; i <= bucket_count; ++i) s_english_buckets[i] = readLe32(data + 28u + i * 4u);
	if (s_english_buckets[0] != 0u || s_english_buckets[26] != count) { GY_free1(data); return 0; }
	for (uint32 i = 0; i < 26; ++i) if (s_english_buckets[i] > s_english_buckets[i + 1]) { GY_free1(data); return 0; }
	EnglishEntry* entries = (EnglishEntry*)GY_malloc1((size_t)count * sizeof(EnglishEntry));
	if (!entries) { GY_free1(data); return 0; }
	uint8* blob = data + header_size + records_size;
	for (uint32 i = 0; i < count; ++i) {
		const uint8* record = data + header_size + (size_t)i * record_size;
		uint32 offset = readLe32(record);
		if (offset >= blob_size || !memchr(blob + offset, '\0', blob_size - offset)) {
			GY_free1(entries); GY_free1(data); gy_log_print("IME English file invalid; direct English input mode\n"); return 0;
		}
		entries[i].word = (const char*)blob + offset; entries[i].frequency = readLe32(record + 4);
		int bucket = entries[i].word[0] - 'a';
		if (bucket < 0 || bucket >= 26 || i < s_english_buckets[bucket] || i >= s_english_buckets[bucket + 1]) {
			GY_free1(entries); GY_free1(data); gy_log_print("IME English file invalid; direct English input mode\n"); return 0;
		}
		for (const char* p = entries[i].word; *p; ++p) if (*p < 'a' || *p > 'z') {
			GY_free1(entries); GY_free1(data); gy_log_print("IME English file invalid; direct English input mode\n"); return 0;
		}
	}
	s_english_words = entries; s_english_count = (int)count; s_english_file_data = data;
	gy_log_print("IME external English dictionary loaded\n");
	return 1;
}
static void freeEnglishDictionary(void)
{
	if (s_english_words) GY_free1(s_english_words);
	if (s_english_file_data) GY_free1(s_english_file_data);
	s_english_words = NULL; s_english_file_data = NULL; s_english_count = 0;
	memset(s_english_buckets, 0, sizeof(s_english_buckets));
}
#else
static int loadEnglishDictionary(void) { return 0; }
static void freeEnglishDictionary(void) { }
#endif

static int hasPrefix(const char* text, const char* prefix)
{
	while (*prefix) { if (*text++ != *prefix++) return 0; }
	return 1;
}
static int phraseMatches(const PhraseEntry* p, const char* query)
{
	/* Split the query into non-empty prefixes of consecutive pinyin syllables. */
	size_t query_len = strlen(query);
	uint8 reachable[IME_QUERY_CAP] = { 0 };
	uint8 next[IME_QUERY_CAP];
	const char* syllable = p->py;
	if (query_len == 0 || query_len >= sizeof(reachable)) return 0;
	reachable[0] = 1;
	while (*syllable) {
		const char* end = strchr(syllable, '\'');
		size_t syllable_len = end ? (size_t)(end - syllable) : strlen(syllable);
		memset(next, 0, sizeof(next));
		for (size_t pos = 0; pos < query_len; ++pos) if (reachable[pos]) {
			for (size_t used = 1; used <= syllable_len && pos + used <= query_len; ++used) {
				if (query[pos + used - 1] != syllable[used - 1]) break;
				if (pos + used == query_len) return 1;
				next[pos + used] = 1;
			}
		}
		memcpy(reachable, next, sizeof(reachable));
		if (!end) break;
		syllable = end + 1;
	}
	return 0;
}

static uint32 phraseCompleteLengths(const PhraseEntry* p, const char* query, size_t query_len)
{
	uint32 reachable = 1u;
	const char* syllable = p->py;
	if (query_len >= IME_QUERY_CAP) query_len = IME_QUERY_CAP - 1;
	while (*syllable) {
		const char* end = strchr(syllable, '\'');
		size_t syllable_len = end ? (size_t)(end - syllable) : strlen(syllable);
		uint32 next = 0;
		for (size_t pos = 0; pos < query_len; ++pos) if (reachable & (1u << pos)) {
			for (size_t used = 1; used <= syllable_len && pos + used <= query_len; ++used) {
				if (query[pos + used - 1] != syllable[used - 1]) break;
				next |= 1u << (pos + used);
			}
		}
		reachable = next;
		if (!reachable || !end) break;
		syllable = end + 1;
	}
	return reachable & ~1u;
}

static GYCTX g_ctx;
static GYOBJ g_input, g_output, g_status;
static GYOBJ g_candidate_view, g_prev, g_next;
static GYOBJ g_keys[26], g_mode, g_ctrl, g_alt, g_shift, g_caps, g_tab, g_backspace, g_space, g_enter;
static GYOBJ g_keyboard, g_settings, g_direction, g_printscreen;
static GYOBJ g_symbol_panel, g_symbol_keys[SYMBOLS_PER_PAGE], g_symbol_categories[9];
static GYOBJ g_symbol_cycle;
static GYOBJ g_num[10];
static GYOBJ g_num_extra[3];
static GYOBJ g_main_punct[8];
static GYOBJ g_function[13];
/* Candidate cache: lazy ranking appends only the pages the user requests. */
#define GY_IME_POOL_CAP 8192
static const char* g_words[GY_IME_POOL_CAP];
static int g_word_count, g_page_start, g_page_count, g_page_index, g_selftest_fail;
static GYcoord g_candidate_x0[PAGE_MAX_CANDIDATES], g_candidate_x1[PAGE_MAX_CANDIDATES];
static int g_english, g_upper, g_shift_latched, g_shift_physical;
static int g_caps_latched, g_ctrl_latched, g_ctrl_physical, g_alt_latched, g_alt_physical;
static int g_key_filter_bypass;
static int g_symbol_mode, g_keyboard_collapsed;
static int g_symbol_category, g_symbol_page_index;
static GYobj_event_cb s_input_event_cb;
static char s_key_chars[26];
static const char* s_key_rows[] = { "qwertyuiop", "asdfghjkl", "zxcvbnm" };
static const char* s_num[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "0" };
static const char* s_num_shift_en[] = { "!", "@", "#", "$", "%", "^", "&", "*", "(", ")" };
static const char* s_num_shift_zh[] = { "！", "@", "#", "￥", "%", "…", "&", "*", "（", "）" };
static const char* s_extra_main_en[] = { "`", "-", "=" };
static const char* s_extra_shift_en[] = { "~", "_", "+" };
static const char* s_extra_main_zh[] = { "·", "-", "=" };
static const char* s_extra_shift_zh[] = { "～", "—", "+" };
static const char* s_punct_main_en[] = { "[", "]", "\\", ";", "'", ",", ".", "/" };
static const char* s_punct_shift_en[] = { "{", "}", "|", ":", "\"", "<", ">", "?" };
static const char* s_punct_main_zh[] = { "【", "】", "、", "；", "‘", "，", "。", "/" };
static const char* s_punct_shift_zh[] = { "〖", "〗", "|", "：", "“", "《", "》", "？" };
static const char* s_function[] = { "Esc", "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12" };
static const char* s_symbol_categories[] = { "常用", "中英", "返回", "数学", "角标", "序号", "希腊", "拼音", "箭头" };
static const char* s_symbol_set_names[] = { "常用", "数学", "角标", "序号", "希腊", "拼音", "箭头" };
static const char* s_english_symbols[SYMBOL_CATEGORY_CAP] = {
	"!","\"","#","$","%","&","'","(",")","*","+",",","-",".","/",":",";","<","=",">","?","@","[","\\","]","^","_","`","{","|","}","~",
	"©","®","™","°","‰","§","¶","†","‡","•","…","′","″","€","£","¥","¢","¤","№","µ","Ω","π","÷","×","±","≠","≤","≥","∞","√","∑","∆","←","↑","→","↓","★","☆","✓","✕"
};
static const char* s_symbol_grid[SYMBOL_CATEGORY_COUNT][SYMBOL_CATEGORY_CAP] = {
	{ "，","。","！","？","；","：","、","…","·","“","”","‘","’","（","）","【","】","《","》","〈","〉","—","～","￥","@","#","%","&","*","+","-","=","/","\\","|","_",
	  "「","」","『","』","〔","〕","〖","〗","｛","｝","［","］","／","＼","｜","＿","＾","｀","＄","￠","￡","€","©","®","™","§","※","☆","★","○","●","◎","◇","◆","□","■" },
	{ "+","-","×","÷","=","≠","≈","<",">","≤","≥","±","%","‰","∞","√","∑","∏","∫","∮","∝","∠","⊥","∥","∪","∩","∈","∉","⊂","⊃","∧","∨","¬","°","′","″",
	  "∓","≡","≅","≌","≒","≔","≕","∛","∜","∂","∇","∆","∬","∭","∀","∃","∄","∋","∌","⊆","⊇","⊄","⊅","⊕","⊖","⊗","⊘","⊙","⊢","⊣","⊤","⋅","∙","⌈","⌉","⌊","⌋" },
	{ "⁰","¹","²","³","⁴","⁵","⁶","⁷","⁸","⁹","⁺","⁻","⁼","⁽","⁾","₀","₁","₂","₃","₄","₅","₆","₇","₈","₉","₊","₋","₌","₍","₎","x²","x³","10ⁿ","m²","m³","℃",
	  "a²","b²","c²","n²","y²","z²","a³","b³","c³","n³","xⁿ","yⁿ","2ⁿ","10²","10³","10⁶","10⁹","mm²","cm²","dm²","km²","mm³","cm³","dm³","km³","㎡","㎥","m/s²","kg/m³","℃","℉" },
	{ "①","②","③","④","⑤","⑥","⑦","⑧","⑨","⑩","⑪","⑫","⑬","⑭","⑮","⑯","⑰","⑱","⑲","⑳","Ⅰ","Ⅱ","Ⅲ","Ⅳ","Ⅴ","Ⅵ","Ⅶ","Ⅷ","Ⅸ","Ⅹ","一","二","三","四","五","六",
	  "⑴","⑵","⑶","⑷","⑸","⑹","⑺","⑻","⑼","⑽","⑾","⑿","⒀","⒁","⒂","⒃","⒄","⒅","⒆","⒇","ⅰ","ⅱ","ⅲ","ⅳ","ⅴ","ⅵ","ⅶ","ⅷ","ⅸ","ⅹ","七","八","九","十","甲","乙","丙","丁" },
	{ "α","β","γ","δ","ε","ζ","η","θ","ι","κ","λ","μ","ν","ξ","ο","π","ρ","σ","τ","υ","φ","χ","ψ","ω","Α","Β","Γ","Δ","Ε","Ζ","Η","Θ","Λ","Π","Σ","Ω",
	  "Ι","Κ","Μ","Ν","Ξ","Ο","Ρ","Τ","Υ","Φ","Χ","Ψ","ς","ϐ","ϑ","ϕ","ϖ","ϱ","ϵ" },
	{ "ā","á","ǎ","à","ē","é","ě","è","ī","í","ǐ","ì","ō","ó","ǒ","ò","ū","ú","ǔ","ù","ǖ","ǘ","ǚ","ǜ","ü","ê","ń","ň","ǹ","ɑ","ɡ","zh","ch","sh","ng","er",
	  "Ā","Á","Ǎ","À","Ē","É","Ě","È","Ī","Í","Ǐ","Ì","Ō","Ó","Ǒ","Ò","Ū","Ú","Ǔ","Ù","Ǖ","Ǘ","Ǚ","Ǜ","Ü","Ê","Ń","Ň","Ǹ" },
	{ "←","↑","→","↓","↖","↗","↘","↙","↔","↕","⇐","⇑","⇒","⇓","⇔","➜","➝","➞","➟","➠","↩","↪","↶","↷","⇦","⇧","⇨","⇩","◀","▲","▶","▼","△","▽","▷","◁",
	  "↚","↛","↜","↝","↞","↟","↠","↡","↢","↣","↤","↥","↦","↧","↫","↬","↭","↮","↯","↰","↱","↲","↳","↴","↵","↼","↽","↾","↿","⇀","⇁","⇄","⇅","⇆","⇇","⇈","⇉","⇊","⇋","⇌","⇍","⇏","⇕","⇖","⇗","⇘","⇙" },
};

static uint32 nextUtf8Codepoint(const char** text)
{
	const uint8* p = (const uint8*)*text;
	uint32 cp;
	if (p[0] < 0x80) { *text += 1; return p[0]; }
	if ((p[0] & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) {
		cp = ((uint32)(p[0] & 0x1F) << 6) | (uint32)(p[1] & 0x3F); *text += 2; return cp;
	}
	if ((p[0] & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
		cp = ((uint32)(p[0] & 0x0F) << 12) | ((uint32)(p[1] & 0x3F) << 6) | (uint32)(p[2] & 0x3F); *text += 3; return cp;
	}
	*text += 1;
	return 0;
}

static int gbFontHasText(const char* text)
{
	while (*text) {
		uint32 cp = nextUtf8Codepoint(&text);
		if (cp == 0 || cp > 0xFFFF) return 0;
		if (cp < 0x80) continue;
		int low = 0, high = YMGUI_GB2312_glyph_count - 1;
		while (low <= high) {
			int mid = low + (high - low) / 2;
			uint32 found = YMGUI_GB2312_cps[mid];
			if (found == cp) break;
			if (found < cp) low = mid + 1; else high = mid - 1;
		}
		if (low > high) return 0;
	}
	return 1;
}

static int symbolCount(int category)
{
	int count = 0;
	if (category < 0 || category >= SYMBOL_CATEGORY_COUNT) return 0;
	const char* const* symbols = (category == 0 && g_english) ? s_english_symbols : s_symbol_grid[category];
	while (count < SYMBOL_CATEGORY_CAP && symbols[count]) count++;
	return count;
}

static const char* symbolAt(int category, int index)
{
	if (category < 0 || category >= SYMBOL_CATEGORY_COUNT || index < 0 || index >= symbolCount(category)) return NULL;
	return (category == 0 && g_english) ? s_english_symbols[index] : s_symbol_grid[category][index];
}

static char g_query[IME_QUERY_CAP];
static uint8 g_lazy_prefix, g_lazy_done;
typedef struct { char word[IME_WORD_CAP]; uint16 hits; } UserStat;
static UserStat s_user_stats[64];
static int s_user_stat_count;
typedef struct { char word[IME_WORD_CAP]; uint32 frequency; } CompositeEntry;
typedef struct { char word[IME_WORD_CAP]; uint32 score; uint8 parts; } ComposeState;
static CompositeEntry s_composites[IME_COMPOSITE_CAP];
static int s_composite_count;
static ComposeState s_compose_states[IME_QUERY_CAP][IME_COMPOSE_BEAM];
static uint8 s_compose_counts[IME_QUERY_CAP];
typedef struct { const PhraseEntry* entry; uint16 hits; } LearnedPhrase;
static LearnedPhrase s_learned_phrases[64];
static int s_learned_phrase_count, s_phrase_cursor, s_phrase_end;
static const PhraseEntry* s_phrase_pending;
static const ImeCharEntry** s_char_matches;
static int s_char_match_count, s_char_cursor;
typedef struct { const EnglishEntry* entry; uint16 hits; } LearnedEnglish;
static LearnedEnglish s_learned_english[64];
static int s_learned_english_count, s_learned_english_cursor;
static int s_english_cursor, s_english_end;

static uint32 composeQuality(uint32 score, uint8 parts)
{
	if (parts == 0) return 0;
	score /= parts;
	for (uint8 i = 1; i < parts; ++i) score = (score + 7u) / 8u;
	return score;
}

static void addComposeState(size_t pos, const char* word, uint32 score, uint8 parts)
{
	ComposeState* states = s_compose_states[pos];
	int count = s_compose_counts[pos];
	for (int i = 0; i < count; ++i) if (strcmp(states[i].word, word) == 0) {
		if (composeQuality(score, parts) > composeQuality(states[i].score, states[i].parts)) { states[i].score = score; states[i].parts = parts; }
		return;
	}
	int slot = count;
	if (count >= IME_COMPOSE_BEAM) {
		slot = 0;
		for (int i = 1; i < count; ++i) if (composeQuality(states[i].score, states[i].parts) < composeQuality(states[slot].score, states[slot].parts)) slot = i;
		if (composeQuality(score, parts) <= composeQuality(states[slot].score, states[slot].parts)) return;
	} else s_compose_counts[pos]++;
	snprintf(states[slot].word, sizeof(states[slot].word), "%s", word);
	states[slot].score = score;
	states[slot].parts = parts;
}

static void buildCompositeCandidates(const char* query)
{
	size_t query_len = strlen(query);
	s_composite_count = 0;
	memset(s_compose_counts, 0, sizeof(s_compose_counts));
	if (query_len < 2 || query_len >= IME_QUERY_CAP) return;
	addComposeState(0, "", 0, 0);
	for (size_t pos = 0; pos < query_len; ++pos) {
		int state_count = s_compose_counts[pos];
		int bucket = query[pos] - 'a';
		int phrase_start = bucket >= 0 && bucket < 26 ? (int)s_phrase_buckets[bucket] : 0;
		int phrase_end = bucket >= 0 && bucket < 26 ? (int)s_phrase_buckets[bucket + 1] : 0;
		int char_start = bucket >= 0 && bucket < 26 ? (int)s_char_buckets[bucket] : 0;
		int char_end = bucket >= 0 && bucket < 26 ? (int)s_char_buckets[bucket + 1] : 0;
		for (int i = phrase_start; i < phrase_end; ++i) {
			uint32 lengths = phraseCompleteLengths(&s_phrases[i], query + pos, query_len - pos);
			if (!lengths) continue;
			for (int state_index = 0; state_index < state_count; ++state_index) {
				ComposeState state = s_compose_states[pos][state_index];
				if (strlen(state.word) + strlen(s_phrases[i].word) + 1 > IME_WORD_CAP) continue;
				char combined[IME_WORD_CAP];
				snprintf(combined, sizeof(combined), "%s%s", state.word, s_phrases[i].word);
				uint32 score = state.score + s_phrases[i].frequency;
				for (size_t used = 1; pos + used <= query_len; ++used)
					if (lengths & (1u << used)) addComposeState(pos + used, combined, score, state.parts + 1);
			}
		}
		for (int i = char_start; i < char_end; ++i) {
			size_t pinyin_len = strlen(s_full_dict[i].py);
			size_t max_used = pinyin_len < query_len - pos ? pinyin_len : query_len - pos;
			size_t matched = 0;
			while (matched < max_used && query[pos + matched] == s_full_dict[i].py[matched]) matched++;
			if (matched == 0) continue;
			for (int state_index = 0; state_index < state_count; ++state_index) {
				ComposeState state = s_compose_states[pos][state_index];
				if (strlen(state.word) + strlen(s_full_dict[i].ch) + 1 > IME_WORD_CAP) continue;
				char combined[IME_WORD_CAP];
				snprintf(combined, sizeof(combined), "%s%s", state.word, s_full_dict[i].ch);
				uint32 score = state.score + s_full_dict[i].frequency;
				for (size_t used = 1; used <= matched; ++used)
					addComposeState(pos + used, combined, score, state.parts + 1);
			}
		}
	}
	for (int i = 0; i < s_compose_counts[query_len] && s_composite_count < IME_COMPOSITE_CAP; ++i) {
		ComposeState* state = &s_compose_states[query_len][i];
		if (state->parts < 2) continue;
		snprintf(s_composites[s_composite_count].word, sizeof(s_composites[0].word), "%s", state->word);
		s_composites[s_composite_count].frequency = composeQuality(state->score, state->parts);
		s_composite_count++;
	}
}

static uint16 wordHits(const char* word)
{
	for (int i = 0; i < s_user_stat_count; ++i) if (strcmp(s_user_stats[i].word, word) == 0) return s_user_stats[i].hits;
	return 0;
}
static int compareCharMatches(const void* left, const void* right)
{
	const ImeCharEntry* a = *(const ImeCharEntry* const*)left;
	const ImeCharEntry* b = *(const ImeCharEntry* const*)right;
	uint16 a_hits = wordHits(a->ch), b_hits = wordHits(b->ch);
	if (a_hits != b_hits) return a_hits > b_hits ? -1 : 1;
	if (a->frequency != b->frequency) return a->frequency > b->frequency ? -1 : 1;
	int pinyin_order = strcmp(a->py, b->py);
	return pinyin_order ? pinyin_order : strcmp(a->ch, b->ch);
}
static void resetCharSearch(const char* query)
{
	s_char_match_count = 0; s_char_cursor = 0;
	if (!s_char_matches) return;
	int bucket = query[0] - 'a';
	if (bucket < 0 || bucket >= 26) return;
	for (int i = (int)s_char_buckets[bucket]; i < (int)s_char_buckets[bucket + 1]; ++i) {
		const ImeCharEntry* entry = &s_full_dict[i];
		if (!hasPrefix(entry->py, query)) continue;
		int duplicate = -1;
		for (int j = 0; j < s_char_match_count; ++j)
			if (strcmp(s_char_matches[j]->ch, entry->ch) == 0) { duplicate = j; break; }
		if (duplicate >= 0) {
			if (entry->frequency > s_char_matches[duplicate]->frequency) s_char_matches[duplicate] = entry;
		} else s_char_matches[s_char_match_count++] = entry;
	}
	qsort(s_char_matches, (size_t)s_char_match_count, sizeof(s_char_matches[0]), compareCharMatches);
}
static int compareEnglishMatches(const void* left, const void* right)
{
	const LearnedEnglish* a = (const LearnedEnglish*)left;
	const LearnedEnglish* b = (const LearnedEnglish*)right;
	if (a->hits != b->hits) return a->hits > b->hits ? -1 : 1;
	if (a->entry->frequency != b->entry->frequency) return a->entry->frequency > b->entry->frequency ? -1 : 1;
	return strcmp(a->entry->word, b->entry->word);
}
static void resetEnglishSearch(const char* query)
{
	s_learned_english_count = 0; s_learned_english_cursor = 0;
	int bucket = query[0] - 'a';
	if (bucket < 0 || bucket >= 26) { s_english_cursor = s_english_end = 0; return; }
	s_english_cursor = (int)s_english_buckets[bucket]; s_english_end = (int)s_english_buckets[bucket + 1];
	if (s_user_stat_count == 0) return;
	for (int i = s_english_cursor; i < s_english_end; ++i) {
		uint16 hits = wordHits(s_english_words[i].word);
		if (!hits || !hasPrefix(s_english_words[i].word, query)) continue;
		if (s_learned_english_count < (int)(sizeof(s_learned_english) / sizeof(s_learned_english[0]))) {
			s_learned_english[s_learned_english_count].entry = &s_english_words[i];
			s_learned_english[s_learned_english_count++].hits = hits;
		}
	}
	qsort(s_learned_english, (size_t)s_learned_english_count, sizeof(s_learned_english[0]), compareEnglishMatches);
}
static void learnWord(const char* word)
{
	for (int i = 0; i < s_user_stat_count; ++i) {
		if (strcmp(s_user_stats[i].word, word) == 0) { if (s_user_stats[i].hits < 65535) s_user_stats[i].hits++; return; }
	}
	if (s_user_stat_count < (int)(sizeof(s_user_stats) / sizeof(s_user_stats[0]))) {
		snprintf(s_user_stats[s_user_stat_count].word, sizeof(s_user_stats[0].word), "%s", word);
		s_user_stats[s_user_stat_count++].hits = 1;
	}
}
static int selectedWord(const char* word)
{
	for (int i = 0; i < g_word_count; ++i) if (strcmp(g_words[i], word) == 0) return 1;
	return 0;
}
static void resetPhraseSearch(const char* query)
{
	s_learned_phrase_count = 0; s_phrase_pending = NULL;
	int bucket = query[0] - 'a';
	if (bucket < 0 || bucket >= 26) { s_phrase_cursor = s_phrase_end = 0; return; }
	s_phrase_cursor = (int)s_phrase_buckets[bucket]; s_phrase_end = (int)s_phrase_buckets[bucket + 1];
	for (int i = s_phrase_cursor; i < s_phrase_end; ++i) {
		const PhraseEntry* phrase = &s_phrases[i];
		uint16 hits = wordHits(phrase->word);
		if (!hits || !phraseMatches(phrase, query)) continue;
		int duplicate = -1;
		for (int j = 0; j < s_learned_phrase_count; ++j)
			if (strcmp(s_learned_phrases[j].entry->word, phrase->word) == 0) { duplicate = j; break; }
		if (duplicate >= 0) {
			if (phrase->frequency > s_learned_phrases[duplicate].entry->frequency) s_learned_phrases[duplicate].entry = phrase;
		} else if (s_learned_phrase_count < (int)(sizeof(s_learned_phrases) / sizeof(s_learned_phrases[0]))) {
			s_learned_phrases[s_learned_phrase_count].entry = phrase;
			s_learned_phrases[s_learned_phrase_count++].hits = hits;
		}
	}
}
static const PhraseEntry* nextStaticPhrase(void)
{
	if (s_phrase_pending && !selectedWord(s_phrase_pending->word)) return s_phrase_pending;
	s_phrase_pending = NULL;
	while (s_phrase_cursor < s_phrase_end) {
		const PhraseEntry* phrase = &s_phrases[s_phrase_cursor++];
		if (!wordHits(phrase->word) && phraseMatches(phrase, g_query) && !selectedWord(phrase->word)) {
			s_phrase_pending = phrase;
			break;
		}
	}
	return s_phrase_pending;
}
static int extendChineseCandidates(int want)
{
	if (want > GY_IME_POOL_CAP) want = GY_IME_POOL_CAP;
	while (g_word_count < want && !g_lazy_done) {
		const char* best = NULL; uint16 best_hits = 0; uint32 best_frequency = 0;
		if (s_char_cursor < s_char_match_count) {
			const ImeCharEntry* entry = s_char_matches[s_char_cursor++];
			best = entry->ch; best_hits = wordHits(entry->ch); best_frequency = entry->frequency;
		}
		if (!best) {
			for (int i = 0; i < s_composite_count; ++i)
				if (!selectedWord(s_composites[i].word)) { uint16 hits = wordHits(s_composites[i].word); if (!best || hits > best_hits || (hits == best_hits && s_composites[i].frequency > best_frequency)) { best = s_composites[i].word; best_hits = hits; best_frequency = s_composites[i].frequency; } }
			for (int i = 0; i < s_learned_phrase_count; ++i) {
				const PhraseEntry* phrase = s_learned_phrases[i].entry;
				uint16 hits = s_learned_phrases[i].hits;
				if (!selectedWord(phrase->word) && (!best || hits > best_hits || (hits == best_hits && phrase->frequency > best_frequency))) { best = phrase->word; best_hits = hits; best_frequency = phrase->frequency; }
			}
			const PhraseEntry* phrase = nextStaticPhrase();
			if (phrase && (!best || (best_hits == 0 && phrase->frequency > best_frequency))) { best = phrase->word; best_hits = 0; best_frequency = phrase->frequency; }
		}
		if (!best) { g_lazy_done = 1; break; }
		g_words[g_word_count++] = best;
		if (s_phrase_pending && strcmp(best, s_phrase_pending->word) == 0) s_phrase_pending = NULL;
	}
	if (g_word_count >= GY_IME_POOL_CAP) g_lazy_done = 1;
	return g_word_count;
}
static int extendPrefixCandidates(int want)
{
	if (!g_english) return extendChineseCandidates(want);
	if (want > GY_IME_POOL_CAP) want = GY_IME_POOL_CAP;
	while (g_word_count < want && s_learned_english_cursor < s_learned_english_count)
		g_words[g_word_count++] = s_learned_english[s_learned_english_cursor++].entry->word;
	while (g_word_count < want && s_english_cursor < s_english_end) {
		const EnglishEntry* entry = &s_english_words[s_english_cursor++];
		if (wordHits(entry->word) || !hasPrefix(entry->word, g_query)) continue;
		g_words[g_word_count++] = entry->word;
	}
	if ((s_learned_english_cursor >= s_learned_english_count && s_english_cursor >= s_english_end) ||
	    g_word_count >= GY_IME_POOL_CAP) g_lazy_done = 1;
	return g_word_count;
}
static void refreshPage(void);
static void refreshNavigationButtons(void);

static GYcoord candidateDrawWidth(const char* word, int page_index)
{
	char number[2] = { (char)('1' + page_index), '\0' };
	return YMGUI_Font_TextWidth(&YMGUI_Font_Default, number) + 3 +
	       YMGUI_Font_TextWidth(&YMGUI_Font_Default, word) + 12;
}

static int pageCountAt(int start, int load_more)
{
	GYcoord used = 8;
	int count = 0;
	GYcoord available = g_candidate_view->area.w;
	while (count < PAGE_MAX_CANDIDATES) {
		int n = start + count;
		if (load_more && n >= g_word_count) extendPrefixCandidates(n + 1);
		if (n >= g_word_count) break;
		GYcoord width = candidateDrawWidth(g_words[n], count);
		if (count > 0 && used + width > available) break;
		used += width;
		count++;
	}
	return count;
}

static void layoutCandidateHits(void)
{
	GYcoord x = g_candidate_view->area.x + 4;
	for (int i = 0; i < PAGE_MAX_CANDIDATES; ++i) g_candidate_x0[i] = g_candidate_x1[i] = 0;
	for (int i = 0; i < g_page_count; ++i) {
		g_candidate_x0[i] = x;
		x += candidateDrawWidth(g_words[g_page_start + i], i);
		g_candidate_x1[i] = x;
	}
}

static void updateCandidates(const char* py)
{
	if (py[0] != '\0') {
		/* Prefix mode is lazy: select only the pages requested by the user. */
		snprintf(g_query, sizeof(g_query), "%s", py);
		if (g_english) for (char* p = g_query; *p; ++p) if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + ('a' - 'A'));
		g_word_count = 0;
		if (g_english) resetEnglishSearch(g_query);
		else {
			resetCharSearch(g_query);
			resetPhraseSearch(g_query);
			buildCompositeCandidates(g_query);
		}
		g_lazy_prefix = 1; g_lazy_done = 0;
	} else { g_word_count = 0; g_lazy_prefix = 0;
	}
	g_page_start = 0; g_page_count = 0; g_page_index = 0;
	refreshPage();
	refreshNavigationButtons();
	char buf[80];
	if (py[0] != '\0' && g_word_count > 0) snprintf(buf, sizeof(buf), g_english ? "英文: %s    第 1 页" : "拼音: %s    第 1 页", py);
	else if (g_english) snprintf(buf, sizeof(buf), S_ENGLISH_COUNT ? "输入英文单词" : "英文词库未安装：字母直接上屏");
	else snprintf(buf, sizeof(buf), "输入拼音，例如 ni / hao / zhong");
	YMGUI_Label_SetText(g_status, buf);
}
static void onInputChanged(GYOBJ input, const char* text) { (void)input; updateCandidates(text); }
static void commitWord(int index)
{
	int n = g_page_start + index;
	if (index < 0 || index >= g_page_count || n >= g_word_count) return;
	const char* selected = g_words[n];
	char cased[IME_WORD_CAP];
	learnWord(selected);
	if (g_english) {
		const char* input = YMGUI_TextInput_GetText(g_input);
		int all_upper = input[0] != '\0';
		for (const char* p = input; *p; ++p) if (*p < 'A' || *p > 'Z') all_upper = 0;
		snprintf(cased, sizeof(cased), "%s", selected);
		if (all_upper) for (char* p = cased; *p; ++p) *p = (char)(*p - ('a' - 'A'));
		else if (input[0] >= 'A' && input[0] <= 'Z') cased[0] = (char)(cased[0] - ('a' - 'A'));
		selected = cased;
	}
	YMGUI_EditView_InsertText(g_output, selected);
	YMGUI_TextInput_SetText(g_input, "");
	updateCandidates("");
	YMGUI_Label_SetText(g_status, "已上屏，可继续输入拼音");
}
static void restoreInputFocus(void) { YMGUI_SetFocus(g_ctx, g_input); }
static void editCompositionKey(uint32 key)
{
	restoreInputFocus();
	g_input->state |= GY_STATE_Editing;
	g_key_filter_bypass = 1;
	YMGUI_Inject_Key(key, 1);
	g_key_filter_bypass = 0;
}
static void editOutputKey(uint32 key)
{
	YMGUI_SetFocus(g_ctx, g_output);
	g_key_filter_bypass = 1;
	if (!(g_output->state & GY_STATE_Editing)) YMGUI_Inject_Key(GY_KEY_ENTER, 1);
	YMGUI_Inject_Key(key, 1);
	g_key_filter_bypass = 0;
	restoreInputFocus();
}
static void candidateEventCb(GYOBJ obj, GYEvent event)
{
	if (event != GY_EVENT_Clicked || g_symbol_mode) return;
	GYcoord x = obj->ctx->point_x;
	for (int i = 0; i < g_page_count; ++i) if (x >= g_candidate_x0[i] && x < g_candidate_x1[i]) {
		commitWord(i);
		restoreInputFocus();
		return;
	}
}
static void commitEnglishInput(void);
static void commitChineseInput(void);
static void appendCommitted(const char* text);
static const char* physicalLiteral(uint32 key, char ascii[2]);
static void onBackspace(GYOBJ button);
static void inputEventCb(GYOBJ obj, GYEvent event)
{
	if (g_key_filter_bypass) {
		s_input_event_cb(obj, event);
		return;
	}
	if (event == GY_EVENT_Key && obj->ctx->last_key == GY_KEY_BACKSPACE) {
		obj->ctx->key_handled = 1;
		onBackspace(g_backspace);
		return;
	}
	if (event == GY_EVENT_Key && !g_symbol_mode) {
		uint32 key = obj->ctx->last_key;
		if (g_page_count > 0 && key >= '1' && key <= '9') {
			int index = (int)(key - '1');
			if (index < g_page_count) {
				obj->ctx->key_handled = 1;
				commitWord(index);
				return;
			}
		}
		if (key >= 0x20 && key <= 0x7E && !((key >= 'a' && key <= 'z') || (key >= 'A' && key <= 'Z'))) {
			char ascii[2];
			obj->ctx->key_handled = 1;
			if (YMGUI_TextInput_GetText(g_input)[0] != '\0') {
				if (g_english) commitEnglishInput(); else commitChineseInput();
			}
			appendCommitted(physicalLiteral(key, ascii));
			restoreInputFocus();
			return;
		}
	}
	s_input_event_cb(obj, event);
}
static void onSubmitted(GYOBJ input, const char* text)
{
	(void)input; (void)text;
	if (g_english) commitEnglishInput(); else commitChineseInput();
}
static void appendCommitted(const char* text)
{
	YMGUI_EditView_InsertText(g_output, text);
}
static void clearOutput(void) { YMGUI_EditView_SetText(g_output, ""); }
static const char* outputText(void) { return YMGUI_EditView_GetText(g_output); }
static void commitEnglishInput(void)
{
	const char* text = YMGUI_TextInput_GetText(g_input);
	if (g_word_count > 0) commitWord(0);
	else if (text[0]) {
		appendCommitted(text);
		YMGUI_TextInput_SetText(g_input, "");
		updateCandidates("");
	}
}
static void commitChineseInput(void)
{
	const char* text = YMGUI_TextInput_GetText(g_input);
	if (g_word_count > 0) commitWord(0);
	else if (text[0]) {
		appendCommitted(text);
		YMGUI_TextInput_SetText(g_input, "");
		updateCandidates("");
		YMGUI_Label_SetText(g_status, "已上屏，可继续输入拼音");
	}
}
static void outputShortcut(uint32 command)
{
	switch (command) {
	case GY_KEY_SEL_ALL: YMGUI_EditView_SelectAll(g_output); break;
	case GY_KEY_COPY: YMGUI_EditView_Copy(g_output); break;
	case GY_KEY_CUT: YMGUI_EditView_Cut(g_output); break;
	case GY_KEY_PASTE: YMGUI_EditView_Paste(g_output); break;
	case GY_KEY_UNDO: YMGUI_EditView_Undo(g_output); break;
	case GY_KEY_FIND:
		YMGUI_SetFocus(g_ctx, g_output); g_key_filter_bypass = 1; YMGUI_Inject_Key(command, 1); g_key_filter_bypass = 0; break;
	default: break;
	}
	restoreInputFocus();
}
static void setSymbolGrid(int page);
static void onKey(GYOBJ button)
{
	for (int i = 0; i < 26; ++i) if (button == g_keys[i]) {
		uint32 key = (uint32)((g_upper != g_caps_latched) ? s_key_chars[i] - ('a' - 'A') : s_key_chars[i]);
		if (g_ctrl_latched || g_ctrl_physical) {
			uint32 command = 0;
			switch (s_key_chars[i]) {
			case 'a': command = GY_KEY_SEL_ALL; break; case 'c': command = GY_KEY_COPY; break;
			case 'x': command = GY_KEY_CUT; break; case 'v': command = GY_KEY_PASTE; break;
			case 'z': command = GY_KEY_UNDO; break; case 'f': command = GY_KEY_FIND; break;
			default: break;
			}
			if (command) outputShortcut(command);
			else if (g_english && S_ENGLISH_COUNT == 0) { char literal[2] = { (char)key, '\0' }; appendCommitted(literal); }
			else editCompositionKey(key);
			restoreInputFocus(); return;
		}
		if (g_english && S_ENGLISH_COUNT == 0) { char literal[2] = { (char)key, '\0' }; appendCommitted(literal); }
		else editCompositionKey(key);
		restoreInputFocus();
		return;
	}
}
static const char* numberShiftText(int index) { return g_english ? s_num_shift_en[index] : s_num_shift_zh[index]; }
static const char* extraKeyText(int index, int shifted) { return g_english ? (shifted ? s_extra_shift_en[index] : s_extra_main_en[index]) : (shifted ? s_extra_shift_zh[index] : s_extra_main_zh[index]); }
static const char* punctuationKeyText(int index, int shifted) { return g_english ? (shifted ? s_punct_shift_en[index] : s_punct_main_en[index]) : (shifted ? s_punct_shift_zh[index] : s_punct_main_zh[index]); }
static const char* physicalLiteral(uint32 key, char ascii[2])
{
	ascii[0] = (char)key; ascii[1] = '\0';
	if (g_english) return ascii;
	for (int i = 0; i < 10; ++i) if ((uint8)s_num_shift_en[i][0] == key) return s_num_shift_zh[i];
	for (int i = 0; i < 3; ++i) {
		if ((uint8)s_extra_main_en[i][0] == key) return s_extra_main_zh[i];
		if ((uint8)s_extra_shift_en[i][0] == key) return s_extra_shift_zh[i];
	}
	for (int i = 0; i < 8; ++i) {
		if ((uint8)s_punct_main_en[i][0] == key) return s_punct_main_zh[i];
		if ((uint8)s_punct_shift_en[i][0] == key) return s_punct_shift_zh[i];
	}
	return ascii;
}
static void refreshKeyboardVisuals(void)
{
	for (int i = 0; i < 26; ++i) {
		char key[2] = { (char)((g_upper != g_caps_latched) ? s_key_chars[i] - ('a' - 'A') : s_key_chars[i]), '\0' };
		YMGUI_Button_SetText(g_keys[i], key);
	}
	for (int i = 0; i < 10; ++i) YMGUI_Obj_Invalidate(g_num[i]);
	for (int i = 0; i < 3; ++i) YMGUI_Obj_Invalidate(g_num_extra[i]);
	for (int i = 0; i < 8; ++i) YMGUI_Obj_Invalidate(g_main_punct[i]);
	YMGUI_Obj_Invalidate(g_shift); YMGUI_Obj_Invalidate(g_caps); YMGUI_Obj_Invalidate(g_ctrl); YMGUI_Obj_Invalidate(g_alt);
}
static void onMode(GYOBJ button)
{
	(void)button; g_english = !g_english;
	YMGUI_TextInput_SetText(g_input, ""); updateCandidates("");
	YMGUI_Button_SetText(g_mode, g_english ? "中文" : "英文");
	refreshKeyboardVisuals();
	if (g_symbol_mode) setSymbolGrid(0);
	YMGUI_Label_SetText(g_status, g_english ? (S_ENGLISH_COUNT ? "英文模式：输入前缀选择单词" : "英文词库未安装：字母直接上屏") : "中文模式：输入拼音选择候选");
	restoreInputFocus();
}
static void onShift(GYOBJ button)
{
	(void)button; g_shift_latched = !g_shift_latched; g_upper = g_shift_latched || g_shift_physical; YMGUI_Button_SetText(g_shift, "Shift");
	refreshKeyboardVisuals();
	restoreInputFocus();
}
static void onModifier(GYOBJ button)
{
	if (button == g_caps) {
		g_caps_latched = !g_caps_latched; refreshKeyboardVisuals();
	} else if (button == g_ctrl) {
		g_ctrl_latched = !g_ctrl_latched;
		YMGUI_Obj_Invalidate(g_ctrl);
	} else if (button == g_alt) {
		g_alt_latched = !g_alt_latched;
		YMGUI_Obj_Invalidate(g_alt);
	}
	restoreInputFocus();
}
static void onTab(GYOBJ button)
{
	(void)button;
	if (YMGUI_TextInput_GetText(g_input)[0] != '\0') {
		if (g_english) commitEnglishInput(); else commitChineseInput();
	}
	appendCommitted("    "); restoreInputFocus();
}
static void onBackspace(GYOBJ button)
{
	(void)button;
	if (YMGUI_TextInput_GetText(g_input)[0] != '\0') {
		editCompositionKey(GY_KEY_BACKSPACE);
	} else {
		editOutputKey(GY_KEY_BACKSPACE); return;
	}
	restoreInputFocus();
}
static void onSpace(GYOBJ button)
{
	(void)button; if (!g_english && g_word_count) commitWord(0); else if (g_english && S_ENGLISH_COUNT) commitEnglishInput(); appendCommitted(" "); restoreInputFocus();
}
static void onEnterKey(GYOBJ button) { (void)button; if (!g_english) commitChineseInput(); else { if (S_ENGLISH_COUNT) commitEnglishInput(); appendCommitted("\n"); } restoreInputFocus(); }
static void refreshSymbolGrid(void)
{
	int count = symbolCount(g_symbol_category);
	int page_count = (count + SYMBOLS_PER_PAGE - 1) / SYMBOLS_PER_PAGE;
	int start = g_symbol_page_index * SYMBOLS_PER_PAGE;
	if (page_count < 1) page_count = 1;
	if (g_symbol_page_index >= page_count) g_symbol_page_index = page_count - 1;
	start = g_symbol_page_index * SYMBOLS_PER_PAGE;
	for (int i = 0; i < SYMBOLS_PER_PAGE; ++i) {
		int item = start + i;
		YMGUI_Obj_SetHidden(g_symbol_keys[i], item >= count);
		if (item < count) YMGUI_Button_SetText(g_symbol_keys[i], symbolAt(g_symbol_category, item));
	}
	YMGUI_Obj_Invalidate(g_candidate_view);
}
static void setSymbolGrid(int category)
{
	if (category < 0 || category >= SYMBOL_CATEGORY_COUNT) return;
	g_symbol_category = category; g_symbol_page_index = 0; refreshSymbolGrid();
}
static void onSymbolKey(GYOBJ button)
{
	for (int i = 0; i < SYMBOLS_PER_PAGE; ++i) if (button == g_symbol_keys[i]) {
		int item = g_symbol_page_index * SYMBOLS_PER_PAGE + i;
		const char* symbol = symbolAt(g_symbol_category, item);
		if (!symbol) return;
		if (!g_english && g_word_count) commitWord(0); else if (g_english && S_ENGLISH_COUNT) commitEnglishInput();
		appendCommitted(symbol); restoreInputFocus(); return;
	}
}
static void onSymbolPrev(GYOBJ button)
{
	(void)button;
	if (g_symbol_page_index > 0) { g_symbol_page_index--; refreshSymbolGrid(); }
	restoreInputFocus();
}
static void onSymbolNext(GYOBJ button)
{
	(void)button;
	int count = symbolCount(g_symbol_category);
	if ((g_symbol_page_index + 1) * SYMBOLS_PER_PAGE < count) { g_symbol_page_index++; refreshSymbolGrid(); }
	restoreInputFocus();
}
static void onSymbolCategory(GYOBJ button)
{
	int index = -1; for (int i = 0; i < 9; ++i) if (button == g_symbol_categories[i]) { index = i; break; }
	if (index == 1) { onMode(g_mode); return; }
	if (index == 2) { g_symbol_mode = 0; YMGUI_Obj_SetHidden(g_symbol_panel, 1); YMGUI_Obj_Invalidate(g_candidate_view); refreshNavigationButtons(); restoreInputFocus(); return; }
	const int pages[] = { 0, -1, -1, 1, 2, 3, 4, 5, 6 };
	if (index >= 0) setSymbolGrid(pages[index]);
	restoreInputFocus();
}
static void onSymbolCycle(GYOBJ button)
{
	(void)button;
	g_symbol_mode = 1; setSymbolGrid(0); YMGUI_Obj_SetHidden(g_symbol_panel, 0); refreshNavigationButtons();
	restoreInputFocus();
}
static void onNumber(GYOBJ button)
{
	for (int i = 0; i < 10; ++i) if (button == g_num[i]) {
		if (!g_upper && g_word_count && i < g_page_count && i < 9) commitWord(i);
		else {
			if (YMGUI_TextInput_GetText(g_input)[0] != '\0') {
				if (g_english) commitEnglishInput(); else commitChineseInput();
			}
			appendCommitted(g_upper ? numberShiftText(i) : s_num[i]);
		}
		restoreInputFocus();
		return;
	}
}
static void onMainPunctuation(GYOBJ button)
{
	for (int i = 0; i < 3; ++i) if (button == g_num_extra[i]) {
		if (YMGUI_TextInput_GetText(g_input)[0] != '\0') {
			if (g_english) commitEnglishInput(); else commitChineseInput();
		}
		appendCommitted(extraKeyText(i, g_upper)); restoreInputFocus(); return;
	}
	for (int i = 0; i < 8; ++i) if (button == g_main_punct[i]) {
		if (YMGUI_TextInput_GetText(g_input)[0] != '\0') {
			if (g_english) commitEnglishInput(); else commitChineseInput();
		}
		appendCommitted(punctuationKeyText(i, g_upper)); restoreInputFocus(); return;
	}
}
static void dualKeyDrawCb(GYOBJ obj, GYSURFACE surface, const GYrect* abs)
{
	YMGUI_Draw_Fill(surface, abs, (obj->state & GY_STATE_Pressed) ? GY_ARGB(0xFF, 0x48, 0x58, 0x68) : GY_ARGB(0xFF, 0x2A, 0x34, 0x40), GY_OPA_COVER);
	GYrect top = { abs->x, abs->y, abs->w, 2 }, bot = { abs->x, abs->y + abs->h - 2, abs->w, 2 };
	GYrect lft = { abs->x, abs->y, 2, abs->h }, rgt = { abs->x + abs->w - 2, abs->y, 2, abs->h };
	GYcolor border = GY_ARGB(0xFF, 0x20, 0x20, 0x20);
	YMGUI_Draw_Fill(surface, &top, border, GY_OPA_COVER); YMGUI_Draw_Fill(surface, &bot, border, GY_OPA_COVER);
	YMGUI_Draw_Fill(surface, &lft, border, GY_OPA_COVER); YMGUI_Draw_Fill(surface, &rgt, border, GY_OPA_COVER);
	const char* main_text = NULL; const char* shifted = NULL;
	for (int i = 0; i < 10; ++i) if (obj == g_num[i]) { main_text = s_num[i]; shifted = numberShiftText(i); break; }
	for (int i = 0; i < 3 && !main_text; ++i) if (obj == g_num_extra[i]) { main_text = extraKeyText(i, 0); shifted = extraKeyText(i, 1); }
	for (int i = 0; i < 8 && !main_text; ++i) if (obj == g_main_punct[i]) { main_text = punctuationKeyText(i, 0); shifted = punctuationKeyText(i, 1); }
	if (!main_text) return;
	GYcolor main_color = g_upper ? GY_ARGB(0xFF, 0x80, 0x88, 0x94) : GY_ARGB(0xFF, 0xFF, 0xFF, 0xFF);
	GYcolor shifted_color = g_upper ? GY_ARGB(0xFF, 0xFF, 0xFF, 0xFF) : GY_ARGB(0xFF, 0x80, 0x88, 0x94);
	YMGUI_Draw_Text(surface, &YMGUI_Font_Default, abs->x + 5, abs->y + 3, shifted, shifted_color);
	YMGUI_Draw_Text(surface, &YMGUI_Font_Default, abs->x + abs->w / 2 - 3, abs->y + 10, main_text, main_color);
}
static void modifierDrawCb(GYOBJ obj, GYSURFACE surface, const GYrect* abs)
{
	GYcolor fill = (obj->state & GY_STATE_Pressed) ? GY_ARGB(0xFF, 0x48, 0x58, 0x68) : GY_ARGB(0xFF, 0x2A, 0x34, 0x40);
	YMGUI_Draw_Fill(surface, abs, fill, GY_OPA_COVER);
	int locked = (obj == g_shift) ? g_upper : (obj == g_caps) ? g_caps_latched :
	             (obj == g_ctrl) ? (g_ctrl_latched || g_ctrl_physical) : (g_alt_latched || g_alt_physical);
	const char* text = obj == g_shift ? "Shift" : (obj == g_caps) ? "Caps" : (obj == g_ctrl) ? "Ctrl" : "Alt";
	GYcolor color = locked ? GY_ARGB(0xFF, 0x00, 0x00, 0x00) : GY_ARGB(0xFF, 0xFF, 0xFF, 0xFF);
	GYcoord tw = YMGUI_Font_TextWidth(&YMGUI_Font_Default, text);
	YMGUI_Draw_Text(surface, &YMGUI_Font_Default, abs->x + (abs->w - tw) / 2, abs->y + (abs->h - 16) / 2, text, color);
}
static void candidateDrawCb(GYOBJ obj, GYSURFACE surface, const GYrect* abs)
{
	GYrect inner = { abs->x + 2, abs->y + 2, abs->w - 4, abs->h - 4 };
	YMGUI_Draw_Fill(surface, abs, GY_ARGB(0xFF, 0x48, 0x90, 0xD8), GY_OPA_COVER);
	YMGUI_Draw_Fill(surface, &inner, GY_ARGB(0xFF, 0x18, 0x24, 0x30), GY_OPA_COVER);
	GYrect saved_clip = surface->clip;
	GYrect view_clip;
	if (!GY_Rect_Intersect(&view_clip, abs, &saved_clip)) return;
	surface->clip = view_clip;
	if (g_symbol_mode) {
		int count = symbolCount(g_symbol_category);
		int pages = (count + SYMBOLS_PER_PAGE - 1) / SYMBOLS_PER_PAGE;
		char text[64];
		if (pages < 1) pages = 1;
		snprintf(text, sizeof(text), "%s%s符号    第 %d/%d 页", g_english ? "英文" : "中文", s_symbol_set_names[g_symbol_category], g_symbol_page_index + 1, pages);
		GYcoord tw = YMGUI_Font_TextWidth(&YMGUI_Font_Default, text);
		YMGUI_Draw_Text(surface, &YMGUI_Font_Default, abs->x + (abs->w - tw) / 2, abs->y + 7, text, GY_ARGB(0xFF, 0xA0, 0xB0, 0xC0));
		surface->clip = saved_clip;
		return;
	}
	GYcoord x = abs->x + 4;
	for (int i = 0; i < g_page_count; ++i) {
		int n = g_page_start + i;
		if (n >= g_word_count) break;
		char number[2] = { (char)('1' + i), '\0' };
		YMGUI_Draw_Text(surface, &YMGUI_Font_Default, x, abs->y + 7, number, GY_ARGB(0xFF, 0x60, 0xD0, 0xFF));
		x += YMGUI_Font_TextWidth(&YMGUI_Font_Default, number) + 3;
		YMGUI_Draw_Text(surface, &YMGUI_Font_Default, x, abs->y + 7, g_words[n], GY_ARGB(0xFF, 0xF0, 0xF0, 0xF0));
		x += YMGUI_Font_TextWidth(&YMGUI_Font_Default, g_words[n]) + 12;
	}
	surface->clip = saved_clip;
	(void)obj;
}
static void refreshPage(void)
{
	g_page_count = g_lazy_prefix ? pageCountAt(g_page_start, 1) : 0;
	layoutCandidateHits();
	if (g_query[0] != '\0' && g_word_count > 0) {
		char status[80];
		snprintf(status, sizeof(status), g_english ? "英文: %s    第 %d 页" : "拼音: %s    第 %d 页", g_query, g_page_index + 1);
		YMGUI_Label_SetText(g_status, status);
	}
	YMGUI_Obj_Invalidate(g_candidate_view);
}
static void refreshNavigationButtons(void)
{
	int idle = !g_symbol_mode && YMGUI_TextInput_GetText(g_input)[0] == '\0';
	YMGUI_Button_SetText(g_prev, idle ? "收起" : "上一页");
	YMGUI_Button_SetText(g_next, idle ? "展开" : "下一页");
}
static void setMainKeyboardHidden(uint8 hidden)
{
	for (int i = 0; i < 13; ++i) YMGUI_Obj_SetHidden(g_function[i], hidden);
	for (int i = 0; i < 10; ++i) YMGUI_Obj_SetHidden(g_num[i], hidden);
	for (int i = 0; i < 3; ++i) YMGUI_Obj_SetHidden(g_num_extra[i], hidden);
	for (int i = 0; i < 26; ++i) YMGUI_Obj_SetHidden(g_keys[i], hidden);
	for (int i = 0; i < 8; ++i) YMGUI_Obj_SetHidden(g_main_punct[i], hidden);
	GYOBJ controls[] = { g_backspace, g_tab, g_caps, g_enter, g_shift, g_mode, g_keyboard, g_settings,
	                   g_ctrl, g_alt, g_symbol_cycle, g_space, g_direction, g_printscreen };
	for (size_t i = 0; i < sizeof(controls) / sizeof(controls[0]); ++i) YMGUI_Obj_SetHidden(controls[i], hidden);
}
static void collapseKeyboard(int collapsed)
{
	g_keyboard_collapsed = collapsed != 0;
	setMainKeyboardHidden((uint8)g_keyboard_collapsed);
	if (g_keyboard_collapsed) {
		g_symbol_mode = 0;
		YMGUI_Obj_SetHidden(g_symbol_panel, 1);
	}
	refreshNavigationButtons();
	restoreInputFocus();
}
static void onPrev(GYOBJ b)
{
	(void)b;
	restoreInputFocus();
	if (g_symbol_mode) { onSymbolPrev(b); return; }
	if (YMGUI_TextInput_GetText(g_input)[0] == '\0') { collapseKeyboard(1); return; }
	if (g_page_start <= 0) return;
	int pos = 0, previous = 0;
	while (pos < g_page_start) {
		int count = pageCountAt(pos, 0);
		if (count <= 0 || pos + count >= g_page_start) { previous = pos; break; }
		previous = pos + count;
		pos += count;
	}
	g_page_start = previous;
	if (g_page_index > 0) g_page_index--;
	refreshPage();
}
static void onNext(GYOBJ b)
{
	(void)b;
	restoreInputFocus();
	if (g_symbol_mode) { onSymbolNext(b); return; }
	if (YMGUI_TextInput_GetText(g_input)[0] == '\0') { collapseKeyboard(0); return; }
	int next = g_page_start + g_page_count;
	if (g_lazy_prefix && next >= g_word_count) extendPrefixCandidates(next + 1);
	if (next >= g_word_count) return;
	g_page_start = next; g_page_index++;
	refreshPage();
}

static void setPhysicalButtonState(GYOBJ button, uint8 pressed)
{
	if (pressed) button->state |= GY_STATE_Pressed;
	else button->state &= (uint8)~GY_STATE_Pressed;
	YMGUI_Obj_Invalidate(button);
}

static GYOBJ letterButton(char letter)
{
	if (letter >= 'A' && letter <= 'Z') letter = (char)(letter + ('a' - 'A'));
	for (int i = 0; i < 26; ++i) if (s_key_chars[i] == letter) return g_keys[i];
	return NULL;
}

static uint8 imeKeyFilter(uint32 key, uint8 pressed, void* user_data)
{
	(void)user_data;
	if (g_key_filter_bypass) return 0;
	if (key == GY_KEY_SHIFT) {
		g_shift_physical = pressed != 0; g_upper = g_shift_latched || g_shift_physical;
		refreshKeyboardVisuals(); return 1;
	}
	if (key == GY_KEY_CTRL) {
		g_ctrl_physical = pressed != 0; YMGUI_Obj_Invalidate(g_ctrl); return 1;
	}
	if (key == GY_KEY_ALT) {
		g_alt_physical = pressed != 0; YMGUI_Obj_Invalidate(g_alt); return 1;
	}
	if (key == GY_KEY_CAPS) {
		if (pressed) onModifier(g_caps);
		return 1;
	}
	if (key == GY_KEY_TAB || key == GY_KEY_BACKSPACE || key == GY_KEY_ENTER) {
		GYOBJ button = key == GY_KEY_TAB ? g_tab : (key == GY_KEY_BACKSPACE ? g_backspace : g_enter);
		setPhysicalButtonState(button, pressed);
		if (pressed) {
			if (key == GY_KEY_TAB) onTab(button);
			else if (key == GY_KEY_BACKSPACE) onBackspace(button);
			else onEnterKey(button);
		}
		return 1;
	}
	if (!pressed) return 0;
	char command_letter = '\0';
	switch (key) {
	case GY_KEY_SEL_ALL: command_letter = 'a'; break; case GY_KEY_COPY: command_letter = 'c'; break;
	case GY_KEY_CUT: command_letter = 'x'; break; case GY_KEY_PASTE: command_letter = 'v'; break;
	case GY_KEY_UNDO: command_letter = 'z'; break; case GY_KEY_FIND: command_letter = 'f'; break;
	default: break;
	}
	if (command_letter != '\0') {
		GYOBJ button = letterButton(command_letter);
		if (button != NULL) onKey(button);
		return 1;
	}
	if (key >= 'a' && key <= 'z') { onKey(letterButton((char)key)); return 1; }
	if (key >= 'A' && key <= 'Z') { onKey(letterButton((char)key)); return 1; }
	if (key == ' ') { onSpace(g_space); return 1; }
	for (int i = 0; i < 10; ++i) if (key == (uint8)s_num[i][0]) { onNumber(g_num[i]); return 1; }
	for (int i = 0; i < 10; ++i) if (key == (uint8)s_num_shift_en[i][0]) { onNumber(g_num[i]); return 1; }
	for (int i = 0; i < 3; ++i) {
		if (key == (uint8)s_extra_main_en[i][0] || key == (uint8)s_extra_shift_en[i][0]) { onMainPunctuation(g_num_extra[i]); return 1; }
	}
	for (int i = 0; i < 8; ++i) {
		if (key == (uint8)s_punct_main_en[i][0] || key == (uint8)s_punct_shift_en[i][0]) { onMainPunctuation(g_main_punct[i]); return 1; }
	}
	return 0;
}

static void selftest(void)
{
	for (int i = 0; i < 10; ++i) if (!gbFontHasText(s_num_shift_zh[i])) {
		gy_log_print("selftest FAIL: Chinese shifted number glyph key=%d text=%s\n", i, s_num_shift_zh[i]);
		g_selftest_fail = 1; return;
	}
	for (int i = 0; i < 3; ++i) if (!gbFontHasText(s_extra_main_zh[i]) || !gbFontHasText(s_extra_shift_zh[i])) {
		gy_log_print("selftest FAIL: Chinese extra punctuation glyph key=%d\n", i);
		g_selftest_fail = 1; return;
	}
	for (int i = 0; i < 8; ++i) if (!gbFontHasText(s_punct_main_zh[i]) || !gbFontHasText(s_punct_shift_zh[i])) {
		gy_log_print("selftest FAIL: Chinese punctuation glyph key=%d\n", i);
		g_selftest_fail = 1; return;
	}
	for (size_t page = 0; page < sizeof(s_symbol_grid) / sizeof(s_symbol_grid[0]); ++page)
		for (int key = 0; key < SYMBOL_CATEGORY_CAP && s_symbol_grid[page][key]; ++key)
			if (!gbFontHasText(s_symbol_grid[page][key])) {
				gy_log_print("selftest FAIL: symbol glyph page=%u key=%u text=%s\n", (unsigned)page, (unsigned)key, s_symbol_grid[page][key]);
				g_selftest_fail = 1; return;
			}
	for (int key = 0; key < SYMBOL_CATEGORY_CAP && s_english_symbols[key]; ++key)
		if (!gbFontHasText(s_english_symbols[key])) {
			gy_log_print("selftest FAIL: English symbol glyph key=%u text=%s\n", (unsigned)key, s_english_symbols[key]);
			g_selftest_fail = 1; return;
		}
	if (S_ENGLISH_COUNT > 0) {
		g_english = 1; YMGUI_TextInput_SetText(g_input, "th"); updateCandidates("th");
		if (g_page_count < 2 || strcmp(g_words[0], "the") != 0) { gy_log_print("selftest FAIL: English frequency candidates\n"); g_selftest_fail = 1; return; }
		char learned_english[32]; snprintf(learned_english, sizeof(learned_english), "%s", g_words[1]); commitWord(1);
		YMGUI_TextInput_SetText(g_input, "th"); updateCandidates("th");
		if (strcmp(g_words[0], learned_english) != 0) { gy_log_print("selftest FAIL: learned English ranking\n"); g_selftest_fail = 1; return; }
		clearOutput();
		YMGUI_TextInput_SetText(g_input, "Th"); updateCandidates("Th"); commitWord(0);
		if (outputText()[0] < 'A' || outputText()[0] > 'Z') { gy_log_print("selftest FAIL: English candidate case\n"); g_selftest_fail = 1; return; }
		clearOutput(); g_english = 0;
	}
	if (S_CHAR_COUNT == 0 || S_PHRASE_COUNT == 0) {
		if (S_CHAR_COUNT > 0) {
		YMGUI_TextInput_SetText(g_input, "wo"); updateCandidates("wo");
		if (g_page_count < 2 || strcmp(g_words[0], "\346\210\221") != 0) { gy_log_print("selftest FAIL: character-only candidates\n"); g_selftest_fail = 1; return; }
		const char* numbered_word = g_words[1]; YMGUI_Inject_Key('2', 1);
		if (strcmp(outputText(), numbered_word) != 0) { gy_log_print("selftest FAIL: character-only selection\n"); g_selftest_fail = 1; return; }
		clearOutput();
		}
		if (S_PHRASE_COUNT > 0) {
			YMGUI_TextInput_SetText(g_input, "wm"); updateCandidates("wm");
			if (g_page_count < 1 || strcmp(g_words[0], "\346\210\221\344\273\254") != 0) { gy_log_print("selftest FAIL: phrase-only candidates\n"); g_selftest_fail = 1; return; }
		}
		gy_log_print("selftest: optional IME dictionary mode OK\n");
		return;
	}
	PhraseEntry women = { "wo'men", "wm", "\346\210\221\344\273\254", 1 };
	const char* valid_spellings[] = { "w", "wo", "wm", "wom", "wme", "wome", "women" };
	for (size_t i = 0; i < sizeof(valid_spellings) / sizeof(valid_spellings[0]); ++i)
		if (!phraseMatches(&women, valid_spellings[i])) { gy_log_print("selftest FAIL: syllable-aware phrase match\n"); g_selftest_fail = 1; return; }
	if (phraseMatches(&women, "wn") || phraseMatches(&women, "wem")) { gy_log_print("selftest FAIL: phrase subsequence rejected\n"); g_selftest_fail = 1; return; }
	YMGUI_TextInput_SetText(g_input, "gada"); updateCandidates("gada");
	if (g_page_count < 1 || strcmp(g_words[0], "嘎达") != 0) { gy_log_print("selftest FAIL: user dictionary gada candidate\n"); g_selftest_fail = 1; return; }
	YMGUI_TextInput_SetText(g_input, "nima"); updateCandidates("nima");
	if (g_page_count < 1 || strcmp(g_words[0], "尼玛") != 0) { gy_log_print("selftest FAIL: user dictionary nima candidate\n"); g_selftest_fail = 1; return; }
	YMGUI_TextInput_SetText(g_input, "gda"); editCompositionKey(GY_KEY_LEFT); editCompositionKey(GY_KEY_LEFT);
	onKey(letterButton('a'));
	if (strcmp(YMGUI_TextInput_GetText(g_input), "gada") != 0) { gy_log_print("selftest FAIL: composition cursor insertion\n"); g_selftest_fail = 1; return; }
	onBackspace(g_backspace);
	if (strcmp(YMGUI_TextInput_GetText(g_input), "gda") != 0) { gy_log_print("selftest FAIL: composition cursor backspace\n"); g_selftest_fail = 1; return; }
	YMGUI_TextInput_SetText(g_input, "wo"); updateCandidates("wo");
	const char* clicked_word = g_words[0];
	GYcoord click_x = g_candidate_x0[0] + 1, click_y = g_candidate_view->area.y + g_candidate_view->area.h / 2;
	YMGUI_Inject_Pointer(click_x, click_y, 1); YMGUI_Inject_Pointer(click_x, click_y, 0);
	if (strcmp(outputText(), clicked_word) != 0) { gy_log_print("selftest FAIL: candidate pointer selection\n"); g_selftest_fail = 1; return; }
	clearOutput();
	YMGUI_TextInput_SetText(g_input, "wo"); updateCandidates("wo");
	if (g_page_count < 2) { gy_log_print("selftest FAIL: candidate number setup\n"); g_selftest_fail = 1; return; }
	const char* numbered_word = g_words[1];
	YMGUI_Inject_Key('2', 1);
	if (strcmp(outputText(), numbered_word) != 0) { gy_log_print("selftest FAIL: candidate keyboard selection\n"); g_selftest_fail = 1; return; }
	clearOutput();
	YMGUI_TextInput_SetText(g_input, "vvvvvv"); updateCandidates("vvvvvv");
	if (g_word_count != 0) { gy_log_print("selftest FAIL: raw composition setup\n"); g_selftest_fail = 1; return; }
	onEnterKey(g_enter);
	if (strcmp(outputText(), "vvvvvv") != 0 || YMGUI_TextInput_GetText(g_input)[0] != '\0') { gy_log_print("selftest FAIL: raw composition enter\n"); g_selftest_fail = 1; return; }
	clearOutput();
	YMGUI_TextInput_SetText(g_input, "vvvv"); updateCandidates("vvvv"); onSubmitted(g_input, "vvvv");
	if (strcmp(outputText(), "vvvv") != 0 || YMGUI_TextInput_GetText(g_input)[0] != '\0') { gy_log_print("selftest FAIL: physical raw composition enter\n"); g_selftest_fail = 1; return; }
	clearOutput();
	YMGUI_EditView_SetText(g_output, "ac");
	YMGUI_Inject_Pointer(g_output->area.x + 12, g_output->area.y + 8, 1);
	YMGUI_Inject_Pointer(g_output->area.x + 12, g_output->area.y + 8, 0);
	YMGUI_EditView_InsertText(g_output, "b");
	if (strcmp(outputText(), "abc") != 0) { gy_log_print("selftest FAIL: output cursor insertion\n"); g_selftest_fail = 1; return; }
	clearOutput(); restoreInputFocus();
	YMGUI_EditView_SetText(g_output, "你好"); YMGUI_TextInput_SetText(g_input, ""); updateCandidates(""); onBackspace(g_backspace);
	if (strcmp(outputText(), "你") != 0) { gy_log_print("selftest FAIL: Chinese empty-composition backspace\n"); g_selftest_fail = 1; return; }
	clearOutput();
	YMGUI_TextInput_SetText(g_input, "ni"); updateCandidates("ni"); YMGUI_SetFocus(g_ctx, g_output); restoreInputFocus(); YMGUI_Inject_Key(GY_KEY_BACKSPACE, 1);
	if (strcmp(YMGUI_TextInput_GetText(g_input), "n") != 0) { gy_log_print("selftest FAIL: physical composition backspace\n"); g_selftest_fail = 1; return; }
	YMGUI_TextInput_SetText(g_input, ""); updateCandidates("");
	YMGUI_TextInput_SetText(g_input, "vvvv"); updateCandidates("vvvv"); YMGUI_Inject_Key(',', 1);
	if (strcmp(outputText(), "vvvv，") != 0 || YMGUI_TextInput_GetText(g_input)[0] != '\0') { gy_log_print("selftest FAIL: physical punctuation direct commit\n"); g_selftest_fail = 1; return; }
	clearOutput(); YMGUI_TextInput_SetText(g_input, "vvvv"); updateCandidates("vvvv"); YMGUI_Inject_Key('7', 1);
	if (strcmp(outputText(), "vvvv7") != 0 || YMGUI_TextInput_GetText(g_input)[0] != '\0') { gy_log_print("selftest FAIL: physical number direct commit\n"); g_selftest_fail = 1; return; }
	YMGUI_EditView_SetText(g_output, "select me"); g_ctrl_latched = 0; onModifier(g_ctrl); onKey(g_keys[10]);
	{
		size_t start, end; YMGUI_EditView_GetSelection(g_output, &start, &end);
		if (start != 0 || end != strlen("select me")) { gy_log_print("selftest FAIL: virtual Ctrl+A output selection\n"); g_selftest_fail = 1; return; }
	}
	onModifier(g_ctrl); clearOutput();
	g_shift_latched = 0; g_shift_physical = 0; g_upper = 0; refreshKeyboardVisuals();
	YMGUI_Inject_Key(GY_KEY_SHIFT, 1); YMGUI_Inject_Key('9', 1);
	if (!g_shift_physical || !g_upper || strcmp(outputText(), "（") != 0) { gy_log_print("selftest FAIL: physical Shift uses virtual key state\n"); g_selftest_fail = 1; return; }
	YMGUI_Inject_Key(GY_KEY_SHIFT, 0); clearOutput();
	onShift(g_shift); YMGUI_Inject_Key(GY_KEY_SHIFT, 1); YMGUI_Inject_Key(GY_KEY_SHIFT, 0);
	if (!g_upper || !g_shift_latched || g_shift_physical) { gy_log_print("selftest FAIL: virtual Shift latch survives physical release\n"); g_selftest_fail = 1; return; }
	onShift(g_shift);
	YMGUI_EditView_SetText(g_output, "physical ctrl"); YMGUI_Inject_Key(GY_KEY_CTRL, 1); YMGUI_Inject_Key(GY_KEY_SEL_ALL, 1);
	{
		size_t start, end; YMGUI_EditView_GetSelection(g_output, &start, &end);
		if (!g_ctrl_physical || start != 0 || end != strlen("physical ctrl")) { gy_log_print("selftest FAIL: physical Ctrl routes virtual command\n"); g_selftest_fail = 1; return; }
	}
	YMGUI_Inject_Key(GY_KEY_CTRL, 0); clearOutput();
	YMGUI_Inject_Key(GY_KEY_CAPS, 1); YMGUI_Inject_Key(GY_KEY_CAPS, 0);
	if (!g_caps_latched) { gy_log_print("selftest FAIL: physical Caps latch\n"); g_selftest_fail = 1; return; }
	YMGUI_Inject_Key(GY_KEY_CAPS, 1); YMGUI_Inject_Key(GY_KEY_CAPS, 0);
	YMGUI_Inject_Key(GY_KEY_TAB, 1);
	if (strcmp(outputText(), "    ") != 0 || !(g_tab->state & GY_STATE_Pressed)) { gy_log_print("selftest FAIL: physical Tab virtual action\n"); g_selftest_fail = 1; return; }
	YMGUI_Inject_Key(GY_KEY_TAB, 0);
	if (g_tab->state & GY_STATE_Pressed) { gy_log_print("selftest FAIL: physical Tab release state\n"); g_selftest_fail = 1; return; }
	clearOutput();
	onPrev(g_prev);
	if (!g_keyboard_collapsed || !(g_function[0]->state & GY_STATE_Hidden) || !(g_symbol_cycle->state & GY_STATE_Hidden)) { gy_log_print("selftest FAIL: idle keyboard collapse\n"); g_selftest_fail = 1; return; }
	onNext(g_next);
	if (g_keyboard_collapsed || (g_function[0]->state & GY_STATE_Hidden) || (g_symbol_cycle->state & GY_STATE_Hidden)) { gy_log_print("selftest FAIL: idle keyboard expand\n"); g_selftest_fail = 1; return; }
	onSymbolCycle(g_symbol_cycle);
	if (!g_symbol_mode) { gy_log_print("selftest FAIL: symbol mode toggle\n"); g_selftest_fail = 1; return; }
	onNext(g_next);
	if (g_symbol_page_index != 1) { gy_log_print("selftest FAIL: symbol next page\n"); g_selftest_fail = 1; return; }
	onSymbolKey(g_symbol_keys[0]);
	if (strcmp(outputText(), symbolAt(0, SYMBOLS_PER_PAGE)) != 0) { gy_log_print("selftest FAIL: symbol page input\n"); g_selftest_fail = 1; return; }
	onPrev(g_prev);
	if (g_symbol_page_index != 0) { gy_log_print("selftest FAIL: symbol previous page\n"); g_selftest_fail = 1; return; }
	onMode(g_mode);
	if (!g_english || strcmp(symbolAt(0, 0), "!") != 0 || g_symbol_page_index != 0) { gy_log_print("selftest FAIL: English symbol mode\n"); g_selftest_fail = 1; return; }
	onMode(g_mode);
	if (g_english || strcmp(symbolAt(0, 0), "，") != 0) { gy_log_print("selftest FAIL: Chinese symbol mode\n"); g_selftest_fail = 1; return; }
	g_symbol_mode = 0; YMGUI_Obj_SetHidden(g_symbol_panel, 1); YMGUI_Obj_Invalidate(g_candidate_view);
	clearOutput();
	onShift(g_shift);
	onNumber(g_num[8]);
	if (!g_upper || strcmp(outputText(), "（") != 0) { gy_log_print("selftest FAIL: Chinese Shift+9 punctuation\n"); g_selftest_fail = 1; return; }
	clearOutput(); onMode(g_mode); onNumber(g_num[8]);
	if (!g_english || !g_upper || strcmp(outputText(), "(") != 0) { gy_log_print("selftest FAIL: Shift preserved across English mode\n"); g_selftest_fail = 1; return; }
	clearOutput(); onMode(g_mode); onShift(g_shift);
	const char* composed_spellings[] = { "kknd", "kankannide" };
	for (size_t i = 0; i < sizeof(composed_spellings) / sizeof(composed_spellings[0]); ++i) {
		YMGUI_TextInput_SetText(g_input, composed_spellings[i]); updateCandidates(composed_spellings[i]);
		if (g_word_count < 1 || strcmp(g_words[0], "\347\234\213\347\234\213\344\275\240\347\232\204") != 0) { gy_log_print("selftest FAIL: composed phrase candidate\n"); g_selftest_fail = 1; return; }
	}
	YMGUI_TextInput_SetText(g_input, "kkn"); updateCandidates("kkn");
	if (g_word_count < 1 || strcmp(g_words[0], "\347\234\213\347\234\213\344\275\240") != 0) { gy_log_print("selftest FAIL: phrase plus character candidate\n"); g_selftest_fail = 1; return; }
	YMGUI_TextInput_SetText(g_input, "ni");
	updateCandidates("ni");
	if (g_page_count < 1 || g_page_count > PAGE_MAX_CANDIDATES || g_word_count < g_page_count) { gy_log_print("selftest FAIL: lazy dynamic first page\n"); g_selftest_fail = 1; return; }
	commitWord(0);
	if (outputText()[0] == '\0') { gy_log_print("selftest FAIL: candidate commit\n"); g_selftest_fail = 1; return; }
	YMGUI_TextInput_SetText(g_input, "zhong");
	updateCandidates("zhong");
	int first_page_count = g_page_count;
	onNext(g_next);
	if (first_page_count < 1 || g_page_start != first_page_count || g_page_count < 1) { gy_log_print("selftest FAIL: dynamic candidate paging\n"); g_selftest_fail = 1; return; }
	onPrev(g_prev);
	if (g_page_start != 0) { gy_log_print("selftest FAIL: dynamic previous page\n"); g_selftest_fail = 1; return; }
	char committed_before[192]; snprintf(committed_before, sizeof(committed_before), "%s", outputText());
	onMode(g_mode); onKey(g_keys[0]);
	if (S_ENGLISH_COUNT > 0) {
		if (strcmp(YMGUI_TextInput_GetText(g_input), "q") != 0 || g_word_count == 0) { gy_log_print("selftest FAIL: virtual English composition\n"); g_selftest_fail = 1; return; }
	} else if (strlen(outputText()) != strlen(committed_before) + 1 || outputText()[strlen(committed_before)] != 'q') { gy_log_print("selftest FAIL: virtual english key\n"); g_selftest_fail = 1; return; }
	onBackspace(g_backspace);
	if ((S_ENGLISH_COUNT > 0 && YMGUI_TextInput_GetText(g_input)[0] != '\0') || strcmp(outputText(), committed_before) != 0) { gy_log_print("selftest FAIL: virtual backspace\n"); g_selftest_fail = 1; return; }
	g_english = 0; YMGUI_TextInput_SetText(g_input, "nihao"); updateCandidates("nihao");
	if (g_word_count < 1 || strcmp(g_words[0], "你好") != 0) { gy_log_print("selftest FAIL: phrase candidate\n"); g_selftest_fail = 1; return; }
	YMGUI_TextInput_SetText(g_input, "a"); updateCandidates("a");
	if (g_word_count == 0) { gy_log_print("selftest FAIL: full GB2312 dictionary\n"); g_selftest_fail = 1; return; }
	YMGUI_TextInput_SetText(g_input, "z"); updateCandidates("z");
	if (g_word_count == 0) { gy_log_print("selftest FAIL: prefix candidates\n"); g_selftest_fail = 1; return; }
	YMGUI_TextInput_SetText(g_input, "zeng"); updateCandidates("zeng");
	while (!g_lazy_done) extendPrefixCandidates(g_word_count + PAGE_MAX_CANDIDATES);
	int found_zeng = 0; for (int i = 0; i < g_word_count; ++i) if (strcmp(g_words[i], "曾") == 0) found_zeng = 1;
	if (!found_zeng) { gy_log_print("selftest FAIL: polyphonic zeng candidate\n"); g_selftest_fail = 1; return; }
	YMGUI_TextInput_SetText(g_input, "k"); updateCandidates("k");
	for (int i = 0; i < g_page_count; ++i) if (strlen(g_words[g_page_start + i]) != 3) { gy_log_print("selftest FAIL: pinyin character priority\n"); g_selftest_fail = 1; return; }
	YMGUI_TextInput_SetText(g_input, "wo"); updateCandidates("wo");
	int wo_index = -1, first_phrase_index = -1, checked = 0;
	while (first_phrase_index < 0 && !g_lazy_done) {
		for (int i = checked; i < g_word_count; ++i) {
			if (strcmp(g_words[i], "\346\262\203") == 0) wo_index = i;
			if (strlen(g_words[i]) > 3) { first_phrase_index = i; break; }
		}
		checked = g_word_count;
		if (first_phrase_index < 0) extendPrefixCandidates(g_word_count + PAGE_MAX_CANDIDATES);
	}
	if (wo_index < 0 || first_phrase_index < 0 || wo_index >= first_phrase_index) { gy_log_print("selftest FAIL: wo characters before phrases\n"); g_selftest_fail = 1; return; }
	YMGUI_TextInput_SetText(g_input, "wm"); updateCandidates("wm");
	if (g_word_count < 1 || strcmp(g_words[0], "我们") != 0) { gy_log_print("selftest FAIL: wm phrase candidate\n"); g_selftest_fail = 1; return; }
	const char* abbrev[] = { "wom", "wome", "wme" };
	for (size_t i = 0; i < sizeof(abbrev) / sizeof(abbrev[0]); ++i) {
		YMGUI_TextInput_SetText(g_input, abbrev[i]); updateCandidates(abbrev[i]);
		if (g_word_count < 1 || strcmp(g_words[0], "\346\210\221\344\273\254") != 0) { gy_log_print("selftest FAIL: phrase abbreviation\n"); g_selftest_fail = 1; return; }
	}
	YMGUI_TextInput_SetText(g_input, "wm"); updateCandidates("wm");
	if (g_page_count < 2) { gy_log_print("selftest FAIL: learned phrase setup\n"); g_selftest_fail = 1; return; }
	char learned_phrase[IME_WORD_CAP]; snprintf(learned_phrase, sizeof(learned_phrase), "%s", g_words[1]); commitWord(1);
	YMGUI_TextInput_SetText(g_input, "wm"); updateCandidates("wm");
	for (int i = 0; i < g_page_count; ++i) if (strcmp(g_words[i], learned_phrase) == 0) { commitWord(i); break; }
	YMGUI_TextInput_SetText(g_input, "wm"); updateCandidates("wm");
	if (strcmp(g_words[0], learned_phrase) != 0) { gy_log_print("selftest FAIL: learned phrase bucket ranking\n"); g_selftest_fail = 1; return; }
	YMGUI_TextInput_SetText(g_input, "ni"); updateCandidates("ni"); char learned[16]; snprintf(learned, sizeof(learned), "%s", g_words[1]); commitWord(1);
	YMGUI_TextInput_SetText(g_input, "ni"); updateCandidates("ni"); for (int i = 0; i < g_word_count; ++i) if (strcmp(g_words[i], learned) == 0) { commitWord(i); break; }
	YMGUI_TextInput_SetText(g_input, "ni"); updateCandidates("ni");
	if (strcmp(g_words[0], learned) != 0) { gy_log_print("selftest FAIL: learned ranking\n"); g_selftest_fail = 1; return; }
	clearOutput();
	g_english = 0; g_upper = 0; g_shift_latched = 0; g_shift_physical = 0; g_caps_latched = 0;
	g_ctrl_latched = 0; g_ctrl_physical = 0; g_alt_latched = 0; g_alt_physical = 0;
	YMGUI_Button_SetText(g_mode, "英文"); YMGUI_Button_SetText(g_shift, "Shift"); refreshKeyboardVisuals();
	g_symbol_mode = 0; YMGUI_Obj_SetHidden(g_symbol_panel, 1);
	YMGUI_Button_SetText(g_symbol_cycle, "符号");
	YMGUI_TextInput_SetText(g_input, "wo"); updateCandidates("wo");
	if (!g_selftest_fail) gy_log_print("selftest: chinese ime candidates + paging OK\n");
}

int main(int argc, char** argv)
{
	GYdisp disp; int max_frames = argc > 1 ? atoi(argv[1]) : -1, frame = 0;
	disp.hor_res = SCR_W; disp.ver_res = SCR_H; disp.buf_px_cnt = SCR_W * BAND_H;
	disp.buf1 = (GYpx*)GY_malloc1(disp.buf_px_cnt * sizeof(GYpx)); disp.buf2 = NULL; disp.user_data = NULL;
	SDL_LCD_Init(&disp, 1);
	g_ctx = YMGUI_Creat_Ctx_Creat(&disp, SCR_W, SCR_H);
	YMGUI_Obj_SetBgColor(g_ctx->root, GY_ARGB(0xFF, 0x12, 0x16, 0x20));
	s_blob = fopen(GB2312_BIN_PATH, "rb");
	if (s_blob) { s_gb_font.glyph_count = YMGUI_GB2312_glyph_count; YMGUI_Font_SetFallback(&s_gb_font); }
#if defined(YMGUI_IME_PHRASES_EXTERNAL)
	if (!loadPhraseDictionary()) memset(s_phrase_buckets, 0, sizeof(s_phrase_buckets));
#else
	loadPhraseDictionary();
#endif
#if defined(YMGUI_IME_CHARS_EXTERNAL)
	if (!loadCharDictionary()) memset(s_char_buckets, 0, sizeof(s_char_buckets));
#else
	loadCharDictionary();
#endif
#if defined(YMGUI_IME_ENGLISH_EXTERNAL)
	if (!loadEnglishDictionary()) memset(s_english_buckets, 0, sizeof(s_english_buckets));
#else
	loadEnglishDictionary();
#endif
	if (S_CHAR_COUNT > 0) s_char_matches = (const ImeCharEntry**)GY_malloc1((size_t)S_CHAR_COUNT * sizeof(*s_char_matches));
	GYOBJ title = YMGUI_Creat_Label_Creat(g_ctx->root, 24, 18, SCR_W - 48, 28);
	YMGUI_Label_SetText(title, "中文拼音输入法"); YMGUI_Label_SetTextColor(title, GY_ARGB(0xFF, 0x70, 0xD8, 0xFF));
	g_output = YMGUI_Creat_EditView_Creat(g_ctx->root, 24, 54, SCR_W - 48, 106, 4096);
	YMGUI_EditView_SetWrap(g_output, 1); YMGUI_EditView_SetPadding(g_output, 4, 4, 4, 4);
	YMGUI_EditView_SetBgColor(g_output, GY_ARGB(0xFF, 0x18, 0x24, 0x30));
	YMGUI_EditView_SetTextColor(g_output, GY_ARGB(0xFF, 0xF0, 0xF0, 0xF0)); YMGUI_EditView_SetText(g_output, "");
	g_status = YMGUI_Creat_Label_Creat(g_ctx->root, 24, 164, 592, 22); YMGUI_Label_SetTextColor(g_status, GY_ARGB(0xFF, 0xA0, 0xB0, 0xC0));
	g_input = YMGUI_Creat_TextInput_Creat(g_ctx->root, 108, 194, 424, 38, 32);
	YMGUI_TextInput_SetChanged(g_input, onInputChanged); YMGUI_TextInput_SetSubmitted(g_input, onSubmitted);
	s_input_event_cb = g_input->event_cb; g_input->event_cb = inputEventCb;
	g_prev = YMGUI_Creat_Button_Creat(g_ctx->root, 24, 194, 72, 38); YMGUI_Button_SetText(g_prev, "上一页"); YMGUI_Button_SetClicked(g_prev, onPrev);
	g_next = YMGUI_Creat_Button_Creat(g_ctx->root, 544, 194, 72, 38); YMGUI_Button_SetText(g_next, "下一页"); YMGUI_Button_SetClicked(g_next, onNext);
	g_candidate_view = YMGUI_Creat_Obj_Creat(g_ctx->root, 24, 238, 592, 30); g_candidate_view->draw_cb = candidateDrawCb; g_candidate_view->event_cb = candidateEventCb;
	g_symbol_cycle = YMGUI_Creat_Button_Creat(g_ctx->root, 134, 438, 64, 28); YMGUI_Button_SetText(g_symbol_cycle, "符号"); YMGUI_Button_SetClicked(g_symbol_cycle, onSymbolCycle);
	for (int i = 0; i < 13; ++i) { GYcoord x = i == 0 ? 24 : 75 + (i - 1) * 45; GYcoord w = i == 0 ? 48 : 42; g_function[i] = YMGUI_Creat_Button_Creat(g_ctx->root, x, 276, w, 26); YMGUI_Button_SetText(g_function[i], s_function[i]); }
	for (int i = 0; i < 10; ++i) { g_num[i] = YMGUI_Creat_Button_Creat(g_ctx->root, 63 + i * 39, 306, 36, 28); YMGUI_Button_SetText(g_num[i], s_num[i]); YMGUI_Button_SetClicked(g_num[i], onNumber); g_num[i]->draw_cb = dualKeyDrawCb; }
	const GYcoord extra_x[] = { 24, 453, 492 };
	for (int i = 0; i < 3; ++i) { g_num_extra[i] = YMGUI_Creat_Button_Creat(g_ctx->root, extra_x[i], 306, 36, 28); YMGUI_Button_SetClicked(g_num_extra[i], onMainPunctuation); g_num_extra[i]->draw_cb = dualKeyDrawCb; }
	g_backspace = YMGUI_Creat_Button_Creat(g_ctx->root, 531, 306, 85, 28); YMGUI_Button_SetText(g_backspace, "Bksp"); YMGUI_Button_SetClicked(g_backspace, onBackspace);
	g_tab = YMGUI_Creat_Button_Creat(g_ctx->root, 24, 338, 54, 28); YMGUI_Button_SetText(g_tab, "Tab"); YMGUI_Button_SetClicked(g_tab, onTab);
	g_caps = YMGUI_Creat_Button_Creat(g_ctx->root, 24, 370, 62, 28); YMGUI_Button_SetText(g_caps, "Caps"); YMGUI_Button_SetClicked(g_caps, onModifier); g_caps->draw_cb = modifierDrawCb;
	g_enter = YMGUI_Creat_Button_Creat(g_ctx->root, 540, 370, 76, 28); YMGUI_Button_SetText(g_enter, "Enter"); YMGUI_Button_SetClicked(g_enter, onEnterKey);
	int key_index = 0;
	for (int row = 0; row < 3; ++row) {
		int len = (int)strlen(s_key_rows[row]);
		GYcoord start = row == 0 ? 81 : (row == 1 ? 89 : 152);
		GYcoord step = row == 2 ? 37 : 41;
		GYcoord width = row == 2 ? 34 : 38;
		for (int col = 0; col < len; ++col) {
			GYOBJ key = YMGUI_Creat_Button_Creat(g_ctx->root, start + col * step, 338 + row * 32, width, 28);
			g_keys[key_index] = key; s_key_chars[key_index] = s_key_rows[row][col]; YMGUI_Button_SetText(key, "a"); YMGUI_Button_SetClicked(key, onKey); key_index++;
		}
	}
	/* The alphabet is stored in row order so callback index maps to a-z. */
	for (int i = 0; i < 26; ++i) { char key[2] = { s_key_chars[i], '\0' }; YMGUI_Button_SetText(g_keys[i], key); }
	const GYcoord punct_x[] = { 491, 532, 573, 458, 499, 411, 448, 485 };
	const GYcoord punct_y[] = { 338, 338, 338, 370, 370, 402, 402, 402 };
	const GYcoord punct_w[] = { 38, 38, 38, 38, 38, 34, 34, 34 };
	for (int i = 0; i < 8; ++i) { g_main_punct[i] = YMGUI_Creat_Button_Creat(g_ctx->root, punct_x[i], punct_y[i], punct_w[i], 28); YMGUI_Button_SetClicked(g_main_punct[i], onMainPunctuation); g_main_punct[i]->draw_cb = dualKeyDrawCb; }
	g_shift = YMGUI_Creat_Button_Creat(g_ctx->root, 24, 402, 64, 28); YMGUI_Button_SetText(g_shift, "Shift"); YMGUI_Button_SetClicked(g_shift, onShift); g_shift->draw_cb = modifierDrawCb;
	g_mode = YMGUI_Creat_Button_Creat(g_ctx->root, 91, 402, 58, 28); YMGUI_Button_SetText(g_mode, "英文"); YMGUI_Button_SetClicked(g_mode, onMode);
	g_keyboard = YMGUI_Creat_Button_Creat(g_ctx->root, 522, 402, 44, 28); YMGUI_Button_SetText(g_keyboard, "键盘");
	g_settings = YMGUI_Creat_Button_Creat(g_ctx->root, 569, 402, 47, 28); YMGUI_Button_SetText(g_settings, "设置");
	g_ctrl = YMGUI_Creat_Button_Creat(g_ctx->root, 24, 438, 52, 28); YMGUI_Button_SetText(g_ctrl, "Ctrl"); YMGUI_Button_SetClicked(g_ctrl, onModifier); g_ctrl->draw_cb = modifierDrawCb;
	g_alt = YMGUI_Creat_Button_Creat(g_ctx->root, 79, 438, 52, 28); YMGUI_Button_SetText(g_alt, "Alt"); YMGUI_Button_SetClicked(g_alt, onModifier); g_alt->draw_cb = modifierDrawCb;
	g_space = YMGUI_Creat_Button_Creat(g_ctx->root, 201, 438, 225, 28); YMGUI_Button_SetText(g_space, "空格"); YMGUI_Button_SetClicked(g_space, onSpace);
	g_direction = YMGUI_Creat_Button_Creat(g_ctx->root, 429, 438, 76, 28); YMGUI_Button_SetText(g_direction, "方向键");
	g_printscreen = YMGUI_Creat_Button_Creat(g_ctx->root, 508, 438, 108, 28); YMGUI_Button_SetText(g_printscreen, "PrintScreen");
	g_symbol_panel = YMGUI_Creat_Obj_Creat(g_ctx->root, 24, 276, 592, 190);
	YMGUI_Obj_SetBgColor(g_symbol_panel, GY_ARGB(0xFF, 0x12, 0x16, 0x20)); g_symbol_panel->state |= GY_STATE_ClipChildren;
	for (int i = 0; i < SYMBOLS_PER_PAGE; ++i) {
		int row = i / 9, col = i % 9;
		g_symbol_keys[i] = YMGUI_Creat_Button_Creat(g_symbol_panel, 10 + col * 64, 8 + row * 38, 60, 32);
		YMGUI_Button_SetText(g_symbol_keys[i], s_symbol_grid[0][i]); YMGUI_Button_SetClicked(g_symbol_keys[i], onSymbolKey);
	}
	for (int i = 0; i < 9; ++i) {
		g_symbol_categories[i] = YMGUI_Creat_Button_Creat(g_symbol_panel, 5 + i * 65, 162, 62, 28);
		YMGUI_Button_SetText(g_symbol_categories[i], s_symbol_categories[i]); YMGUI_Button_SetClicked(g_symbol_categories[i], onSymbolCategory);
	}
	setSymbolGrid(0);
	YMGUI_Obj_SetHidden(g_symbol_panel, 1);
	YMGUI_Inject_SetCtx(g_ctx); YMGUI_Inject_SetKeyFilter(imeKeyFilter, NULL); YMGUI_SetFocus(g_ctx, g_input);
	updateCandidates("");
	if (max_frames > 0) selftest();
	while (SDL_LCD_PumpEvents()) { YMGUI_Refresh(g_ctx); SDL_LCD_Delay(16); if (max_frames > 0 && ++frame >= max_frames) break; }
	YMGUI_Inject_SetKeyFilter(NULL, NULL); YMGUI_Free_CtxFree(g_ctx); SDL_LCD_Destroy();
	if (s_char_matches) GY_free1(s_char_matches);
	freeEnglishDictionary(); freeCharDictionary(); freePhraseDictionary(); if (s_blob) fclose(s_blob); GY_free1(disp.buf1);
	gy_log_print("chinese_ime exit ok\n"); return g_selftest_fail ? 1 : 0;
}
