/* 应用共用的候选引擎实现。每个可执行程序一份状态；不含 GUI/SDL。 */
#ifndef YAOMI_IME_CANDIDATES_INC
#define YAOMI_IME_CANDIDATES_INC
#include "YMGUI_PubDefine.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define IME_QUERY_CAP 32
#define IME_WORD_CAP 96
#define IME_COMPOSITE_CAP 12
#define IME_COMPOSE_BEAM 6
#define GY_IME_POOL_CAP 8192
static const char* g_words[GY_IME_POOL_CAP];
static int g_word_count, g_english;
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
static void imeCandidatesReset(const char* py)
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
	if (!py[0]) { g_query[0] = 0; g_lazy_done = 1; }
}
#endif
