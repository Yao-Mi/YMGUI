#include "audio.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

#define TAU 6.283185307179586
#define BLOCK 1024
static float clamp(float v, float lo, float hi)
{
	return v < lo ? lo : v > hi ? hi
								: v;
}
static float wave(const MsSounds* s, double phase)
{
	return s->sine[(uint64_t)(phase * 4096.0) & 4095];
}
/* WAV 加载只接受应用随附的 48 kHz 单声道 16 位 PCM，最长五秒。 */
static int load_drum(MsSounds* s, int drum, const char* path)
{
	SDL_AudioSpec spec;
	Uint8* data = NULL;
	Uint32 bytes = 0;
	if (!SDL_LoadWAV(path, &spec, &data, &bytes) || spec.freq != MS_RATE ||
		spec.channels != 1 || spec.format != AUDIO_S16LSB || !bytes || bytes % 2 || bytes > MS_RATE * 10)
	{
		SDL_FreeWAV(data);
		return 0;
	}
	s->lengths[drum] = (int)(bytes / 2);
	s->drums[drum] = malloc((size_t)s->lengths[drum] * sizeof(float));
	if (!s->drums[drum])
	{
		SDL_FreeWAV(data);
		return 0;
	}
	for (int i = 0; i < s->lengths[drum]; ++i)
	{
		int value = data[i * 2] | (int)data[i * 2 + 1] << 8;
		if (value >= 32768)
			value -= 65536;
		s->drums[drum][i] = value / 32768.f;
	}
	SDL_FreeWAV(data);
	return 1;
}
int ms_sounds_init(MsSounds* s)
{
	memset(s, 0, sizeof(*s));
	/* 正弦表仅供节拍器提示音；全部乐器和鼓声来自 PCM 资源。 */
	for (int i = 0; i < 4096; ++i)
		s->sine[i] = sinf((float)(TAU * i / 4096.0));
	static const char* banks[] = {
		"piano", "electric_piano", "bass", "guitar", "strings", "organ", "flute",
		"lead", "horn", "brass", "timpani", "voice", "choir"
	};
	static const char* drums[] = {"kick", "snare", "closed_hat", "open_hat", "clap", "low_tom", "high_tom", "crash", "snap"};
	char path[MS_PATH];
	for (int i = 0; i < MS_INSTRUMENTS; ++i)
	{
		snprintf(path, sizeof(path), "%s/%s.bank", MS_SAMPLE_DIR, banks[i]);
		if (!ms_sample_bank_load(s->instrument[i], path))
			goto failed;
	}
	for (int i = 0; i < MS_DRUMS; ++i)
	{
		snprintf(path, sizeof(path), "%s/%s.wav", i == 4 || i == 8 ? MS_BODY_DIR : MS_SAMPLE_DIR, drums[i]);
		if (!load_drum(s, i, path))
			goto failed;
	}
	return 1;
failed:
	fprintf(stderr, "无法加载采样音源：%s\n", path);
	ms_sounds_free(s);
	return 0;
}
void ms_sounds_free(MsSounds* s)
{
	for (int i = 0; i < MS_DRUMS; ++i)
		free(s->drums[i]);
	for (int voice = 0; voice < MS_INSTRUMENTS; ++voice)
		for (int zone = 0; zone < MS_SAMPLE_ZONES; ++zone)
			free(s->instrument[voice][zone]);
	memset(s, 0, sizeof(*s));
}
/* FFmpeg 仅作应用侧解码器，通过 argv 传路径，不经过 shell。 */
int ms_asset_import(MsAsset* a, const char* path, char* error, size_t cap)
{
	memset(a, 0, sizeof(*a));
	int pipes[2];
	if (pipe(pipes))
	{
		snprintf(error, cap, "无法创建解码管道");
		return 0;
	}
	pid_t child = fork();
	if (child == 0)
	{
		close(pipes[0]);
		dup2(pipes[1], STDOUT_FILENO);
		close(pipes[1]);
		execlp("ffmpeg", "ffmpeg", "-nostdin", "-v", "error", "-i", path, "-t", "121", "-vn", "-f", "f32le", "-acodec", "pcm_f32le", "-ar", "48000", "-ac", "2", "pipe:1", (char*)NULL);
		_exit(127);
	}
	close(pipes[1]);
	if (child < 0)
	{
		close(pipes[0]);
		snprintf(error, cap, "无法启动解码器");
		return 0;
	}
	size_t max = (size_t)MS_ASSET_FRAMES * 8, used = 0, capacity = 65536;
	unsigned char* data = malloc(capacity);
	int ok = data != NULL;
	while (ok)
	{
		if (used == capacity)
		{
			size_t next = capacity * 2;
			if (next > max + 8)
				next = max + 8;
			if (next <= capacity)
			{
				ok = 0;
				break;
			}
			void* mem = realloc(data, next);
			if (!mem)
			{
				ok = 0;
				break;
			}
			data = mem;
			capacity = next;
		}
		ssize_t n = read(pipes[0], data + used, capacity - used);
		if (n < 0 && errno == EINTR)
			continue;
		if (n <= 0)
		{
			if (n < 0)
				ok = 0;
			break;
		}
		used += (size_t)n;
		if (used > max)
		{
			ok = 0;
			break;
		}
	}
	close(pipes[0]);
	if (!ok)
		kill(child, SIGTERM);
	int status = 0;
	while (waitpid(child, &status, 0) < 0 && errno == EINTR)
	{
	}
	if (!ok || !WIFEXITED(status) || WEXITSTATUS(status) || !used || used % 8)
	{
		free(data);
		snprintf(error, cap, "导入失败：请检查 FFmpeg、音频格式及单文件 120 秒限制");
		return 0;
	}
	a->frames = (int)(used / 8);
	a->pcm = (float*)data;
	for (size_t i = 0; i < used / 4; ++i)
	{
		uint32_t v = (uint32_t)data[4 * i] | (uint32_t)data[4 * i + 1] << 8 | (uint32_t)data[4 * i + 2] << 16 | (uint32_t)data[4 * i + 3] << 24;
		float sample;
		memcpy(&sample, &v, 4);
		a->pcm[i] = isfinite(sample) ? clamp(sample, -1, 1) : 0;
	}
	const char* base = strrchr(path, '/');
	base = base ? base + 1 : path;
	size_t n = strlen(base);
	if (n >= MS_NAME)
	{
		n = MS_NAME - 1;
		while (n && ((unsigned char)base[n] & 0xc0) == 0x80)
			--n;
	}
	memcpy(a->name, base, n);
	a->name[n] = 0;
	ms_asset_peaks(a);
	return 1;
}
static void voice(const MsSounds* sounds, const MsClip* clip, float* bus, int count, int64_t frame, int64_t event,
				  int64_t clip_begin, int64_t clip_end, const float* pcm, int length, int stereo,
				  int pitch, int instrument, float volume, float pan)
{
	int lo = 0, hi = count;
	if (event > frame)
		lo = (int)fmin(count, event - frame);
	if (clip_begin > frame + lo)
		lo = (int)fmin(count, clip_begin - frame);
	if (event + length < frame + hi)
		hi = (int)fmax(0, event + length - frame);
	if (clip_end < frame + hi)
		hi = (int)fmax(0, clip_end - frame);
	if (lo >= hi)
		return;
	float left = volume * sqrtf((1 - pan) * .5f), right = volume * sqrtf((1 + pan) * .5f);
	double hz = pcm ? 0 : 440 * pow(2, (pitch - 69) / 12.0);
	for (int i = lo; i < hi; ++i)
	{
		int pos = (int)(frame + i - event);
		float l, r;
		if (pcm)
		{
			l = pcm[pos * (stereo ? 2 : 1)];
			r = stereo ? pcm[pos * 2 + 1] : l;
		}
		else
		{
			l = r = ms_instrument_sample(sounds, hz, instrument, pos, length);
		}
		float fade = 1;
		if (clip->fade_in > 0)
			fade *= clamp((float)(frame + i - clip_begin) / (clip->fade_in * MS_RATE), 0, 1);
		if (clip->fade_out > 0)
			fade *= clamp((float)(clip_end - frame - i) / (clip->fade_out * MS_RATE), 0, 1);
		bus[i * 2] += l * left * fade;
		bus[i * 2 + 1] += r * right * fade;
	}
}
static void render_block(const MsProject* p, const MsSounds* sounds, int64_t frame, int n, float* out, float* meters, int metro)
{
	const MsSong* s = &p->song;
	memset(out, 0, (size_t)n * 2 * sizeof(float));
	int solo = 0, drum_solo = 0;
	for (int t = 0; t < s->tracks; ++t)
		solo |= s->track[t].solo;
	for (int d = 0; d < MS_DRUMS; ++d)
		drum_solo |= s->drum_solo[d];
	for (int t = 0; t < s->tracks; ++t)
	{
		const MsTrack* track = &s->track[t];
		if (track->mute || (solo && !track->solo))
			continue;
		float bus[BLOCK * 2] = {0};
		for (int c = 0; c < s->clips; ++c)
		{
			const MsClip* clip = &s->clip[c];
			if (clip->track != t)
				continue;
			int64_t begin = llround(ms_tick_frame(s, clip->start)), end = llround(ms_tick_frame(s, clip->start + clip->length));
			if (frame >= end || frame + n <= begin)
				continue;
			int64_t source_begin = llround(ms_tick_frame(s, clip->start - clip->offset));
			if (track->kind == MS_AUDIO)
			{
				if (clip->source < 0 || clip->source >= p->assets)
					continue;
				const MsAsset* a = &p->asset[clip->source];
				voice(sounds, clip, bus, n, frame, source_begin, begin, end, a->pcm, a->frames, 1, 0, 0, clip->gain, 0);
			}
			else
			{
				const MsPattern* pat = &s->pattern[clip->source];
				int period = pat->steps * MS_STEP;
				double source_tick = ms_frame_tick(s, frame) - clip->start + clip->offset;
				/* 考虑前几个周期中尚未结束的采样尾音；不生成负周期。 */
				int first = (int)floor((source_tick - ms_frame_tick(s, MS_ASSET_FRAMES)) / period);
				int last = (int)floor((ms_frame_tick(s, frame + n) - clip->start + clip->offset) / period);
				if (first < 0)
					first = 0;
				/* 按实际采样尾长回看旧周期，避免吊镲长尾在节奏重复时被漏掉。 */
				int tail_frames = 0;
				for (int d = 0; d < MS_DRUMS; ++d)
					if (sounds->lengths[d] > tail_frames)
						tail_frames = sounds->lengths[d];
				int max_tail = (int)ceil(ms_frame_tick(s, tail_frames) / period) + 1;
				int custom = 0;
				for (int d = 0; d < MS_DRUMS; ++d)
					custom |= s->drum_asset[d] >= 0;
				if ((!custom || track->kind == MS_SYNTH) && first < last - max_tail)
					first = last - max_tail;
				if (first < 0)
					first = 0;
				for (int cycle = first; cycle <= last; ++cycle)
				{
					if (track->kind == MS_DRUM)
					{
						for (int d = 0; d < MS_DRUMS; ++d)
						{
							if (s->drum_mute[d] || (drum_solo && !s->drum_solo[d]))
								continue;
							int a = s->drum_asset[d];
							const float* sample = a >= 0 ? p->asset[a].pcm : sounds->drums[d];
							int length = a >= 0 ? p->asset[a].frames : sounds->lengths[d];
							for (int step = 0; step < pat->steps; ++step)
								if (pat->drum[d][step])
								{
									int64_t event = llround(ms_tick_frame(s, clip->start - clip->offset + cycle * period + step * MS_STEP));
									voice(sounds, clip, bus, n, frame, event, begin, end, sample, length, a >= 0, 0, 0,
										  clip->gain * s->drum_volume[d] * pat->drum[d][step] / 127.f, s->drum_pan[d]);
								}
						}
					}
					else
						for (int note = 0; note < pat->count; ++note)
						{
							const MsNote* v = &pat->notes[note];
							int64_t event = llround(ms_tick_frame(s, clip->start - clip->offset + cycle * period + v->tick));
							voice(sounds, clip, bus, n, frame, event, begin, end, NULL, (int)llround(ms_tick_frame(s, v->length)), 0,
								  v->pitch, track->instrument, clip->gain * v->velocity / 127.f, 0);
						}
				}
			}
		}
		float l = track->volume * sqrtf(1 - track->pan), r = track->volume * sqrtf(1 + track->pan);
		for (int i = 0; i < n; ++i)
		{
			float a = bus[2 * i] * l, b = bus[2 * i + 1] * r;
			out[i * 2] += a;
			out[i * 2 + 1] += b;
			if (meters)
				meters[t] = fmaxf(meters[t], fmaxf(fabsf(a), fabsf(b)));
		}
	}
	for (int i = 0; i < n; ++i)
	{
		float click = 0;
		if (metro)
		{
			double beat = ms_frame_tick(s, frame + i) / MS_PPQ;
			double sec = (beat - floor(beat)) * 60 / s->bpm;
			if (sec < .025)
				click = .13f * wave(sounds, sec * ((int)beat % s->beats ? 1200 : 1800)) * (float)(1 - sec / .025);
		}
		for (int ch = 0; ch < 2; ++ch)
		{
			float v = out[i * 2 + ch] * s->master + click;
			if (meters)
				meters[MS_TRACKS] = fmaxf(meters[MS_TRACKS], fabsf(v));
			out[i * 2 + ch] = clamp(v, -1, 1);
		}
	}
}
void ms_render(const MsProject* p, const MsSounds* sounds, int64_t frame, int frames, float* out, float* meters, int metro)
{
	if (meters)
		memset(meters, 0, (MS_TRACKS + 1) * sizeof(float));
	for (int off = 0; off < frames;)
	{
		int n = frames - off;
		if (n > BLOCK)
			n = BLOCK;
		render_block(p, sounds, frame + off, n, out + off * 2, meters, metro);
		off += n;
	}
}
static void put32(FILE* f, uint32_t v)
{
	unsigned char b[] = {v, v >> 8, v >> 16, v >> 24};
	fwrite(b, 1, 4, f);
}
int ms_export(const MsProject* p, const MsSounds* sounds, const char* path, int loop_only, char* error, size_t cap)
{
	if (!ms_song_valid(&p->song, p->assets) || strlen(path) >= MS_PATH)
	{
		snprintf(error, cap, "工程或路径无效");
		return 0;
	}
	int64_t first = llround(ms_tick_frame(&p->song, loop_only ? p->song.loop_start : 0));
	int64_t end = llround(ms_tick_frame(&p->song, loop_only ? p->song.loop_end : ms_song_end(&p->song)));
	char tmp[MS_PATH + 32];
	snprintf(tmp, sizeof(tmp), "%s.tmp.XXXXXX", path);
	int fd = mkstemp(tmp);
	FILE* f = fd >= 0 ? fdopen(fd, "wb") : NULL;
	if (!f)
	{
		if (fd >= 0)
		{
			close(fd);
			unlink(tmp);
		}
		snprintf(error, cap, "无法创建导出文件");
		return 0;
	}
	uint32_t size = (uint32_t)((end - first) * 4);
	fwrite("RIFF", 1, 4, f);
	put32(f, size + 36);
	fwrite("WAVEfmt ", 1, 8, f);
	put32(f, 16);
	fwrite("\1\0\2\0", 1, 4, f);
	put32(f, MS_RATE);
	put32(f, MS_RATE * 4);
	fwrite("\4\0\20\0data", 1, 8, f);
	put32(f, size);
	for (int64_t frame = first; frame < end && !ferror(f); frame += BLOCK)
	{
		int n = (int)fmin(BLOCK, end - frame);
		float data[BLOCK * 2];
		unsigned char pcm16[BLOCK * 4];
		ms_render(p, sounds, frame, n, data, NULL, 0);
		for (int i = 0; i < n * 2; ++i)
		{
			int16_t v = (int16_t)lrintf(data[i] * 32767);
			pcm16[i * 2] = v;
			pcm16[i * 2 + 1] = (uint16_t)v >> 8;
		}
		fwrite(pcm16, 4, (size_t)n, f);
	}
	int ok = !ferror(f) && fflush(f) == 0 && fsync(fd) == 0;
	if (fclose(f))
		ok = 0;
	if (ok && rename(tmp, path) == 0)
		return 1;
	unlink(tmp);
	snprintf(error, cap, "导出失败，原文件未替换");
	return 0;
}
/* GUI 刷新和保存不能决定音频补给时机。工作线程持有模型值快照，
 * 借用只读 PCM；调用者必须先 seek/close，再释放工程素材或音源。 */
struct MsPlayback
{
	SDL_Thread* thread;
	SDL_mutex* mutex;
	SDL_atomic_t stop;
	MsProject project;
	const MsSounds* sounds;
	SDL_AudioDeviceID device;
	int64_t cursor, begin, end;
	int looping, metronome, done, failed;
	float meters[MS_TRACKS + 1];
};
#define QUEUE_FRAMES (BLOCK * 4)

/* 启动前同步预填，运行时由工作线程独占调用；共享状态在 mutex 下访问。 */
static void playback_fill(MsPlayback* w)
{
	while (!SDL_AtomicGet(&w->stop) && SDL_GetQueuedAudioSize(w->device) < QUEUE_FRAMES * 8)
	{
		if (!w->looping && w->cursor >= w->end)
		{
			w->done = !SDL_GetQueuedAudioSize(w->device);
			break;
		}
		int64_t frame = w->cursor;
		if (w->looping && frame >= w->end)
			frame = w->begin + (frame - w->end) % (w->end - w->begin);
		int n = (int)fmin(BLOCK, w->end - frame);
		float data[BLOCK * 2];
		ms_render(&w->project, w->sounds, frame, n, data, w->meters, w->metronome);
		if (SDL_QueueAudio(w->device, data, (Uint32)n * 8))
		{
			w->failed = w->done = 1;
			break;
		}
		w->cursor += n;
	}
}
static int playback_worker(void* user)
{
	MsPlayback* w = user;
	while (!SDL_AtomicGet(&w->stop))
	{
		SDL_LockMutex(w->mutex);
		playback_fill(w);
		int done = w->done;
		SDL_UnlockMutex(w->mutex);
		if (done)
			break;
		SDL_Delay(2);
	}
	return 0;
}
static void playback_stop(MsTransport* t)
{
	MsPlayback* w = t->playback;
	if (!w)
		return;
	SDL_AtomicSet(&w->stop, 1);
	if (w->thread)
		SDL_WaitThread(w->thread, NULL);
	t->cursor = w->cursor;
	if (w->mutex)
		SDL_DestroyMutex(w->mutex);
	/* project.asset 的 PCM 归原工程所有，不调用 ms_project_free。 */
	free(w);
	t->playback = NULL;
}
void ms_transport_init(MsTransport* t)
{
	memset(t, 0, sizeof(*t));
	t->clip = -1;
	SDL_InitSubSystem(SDL_INIT_AUDIO);
	SDL_AudioSpec want = {0};
	want.freq = MS_RATE;
	want.format = AUDIO_F32SYS;
	want.channels = 2;
	want.samples = BLOCK;
	t->device = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);
	/* 默认暂停，预填完成后才开始消费，避免起音时队列为空。 */
	t->loop = 1;
}
void ms_transport_close(MsTransport* t)
{
	playback_stop(t);
	if (t->device)
		SDL_CloseAudioDevice(t->device);
	memset(t, 0, sizeof(*t));
}
int64_t ms_transport_position(const MsTransport* t)
{
	MsPlayback* w = t->playback;
	if (w)
		SDL_LockMutex(w->mutex);
	int64_t cursor = w ? w->cursor : t->cursor;
	int64_t queued = t->device && (w || t->playing) ? SDL_GetQueuedAudioSize(t->device) / 8 : 0;
	if (w)
		SDL_UnlockMutex(w->mutex);
	int64_t pos = cursor > queued ? cursor - queued : 0;
	if (t->looping && t->loop_last > t->loop_first && pos >= t->loop_last)
		pos = t->loop_first + (pos - t->loop_last) % (t->loop_last - t->loop_first);
	return pos;
}
void ms_transport_seek(MsTransport* t, int64_t frame)
{
	if (t->device)
		SDL_PauseAudioDevice(t->device, 1);
	playback_stop(t);
	if (t->device)
		SDL_ClearQueuedAudio(t->device);
	t->cursor = frame < 0 ? 0 : frame;
	t->looping = 0;
	memset(t->meters, 0, sizeof(t->meters));
}
int ms_preview_project(MsProject* preview, const MsProject* source, int pattern, int track, int clip)
{
	const MsSong* original = &source->song;
	int selected = clip >= 0 && clip < original->clips && original->clip[clip].track == track &&
		(original->track[track].kind == MS_AUDIO || original->clip[clip].source == pattern);
	if (!selected && (pattern < 0 || pattern >= original->patterns))
		return 0;
	int kind = selected ? original->track[track].kind : original->pattern[pattern].kind;
	if (track < 0 || track >= original->tracks || original->track[track].kind != kind)
	{
		for (track = 0; track < original->tracks; ++track)
			if (original->track[track].kind == kind)
				break;
	}
	if (track == original->tracks)
		return 0;
	/* 借用 PCM；已编排片段保留偏移、长度、增益与淡化，只把起点平移到零。 */
	*preview = *source;
	MsSong* s = &preview->song;
	s->clips = 0;
	for (int i = 0; i < s->tracks; ++i)
		s->track[i].solo = 0;
	s->track[track].mute = 0;
	if (selected)
	{
		s->clip[0] = original->clip[clip];
		s->clip[0].start = 0;
		s->clips = 1;
	}
	else
		ms_clip_add(s, track, pattern, 0, s->pattern[pattern].steps * MS_STEP);
	s->loop_start = 0;
	s->loop_end = s->clip[0].length;
	return 1;
}
void ms_transport_tick(MsTransport* t, const MsProject* p, const MsSounds* sounds)
{
	if (!t->device)
		return;
	if (!t->playing)
	{
		if (t->playback)
			ms_transport_seek(t, ms_transport_position(t));
		return;
	}
	MsPlayback* w = t->playback;
	if (!w)
	{
		SDL_PauseAudioDevice(t->device, 1);
		SDL_ClearQueuedAudio(t->device);
		w = calloc(1, sizeof(*w));
		if (!w)
		{
			t->playing = 0;
			return;
		}
		t->playback = w;
		w->cursor = t->cursor;
		w->mutex = SDL_CreateMutex();
		if (!w->mutex)
			goto failed;
		if (t->pattern_mode)
		{
			if (!ms_preview_project(&w->project, p, t->pattern, t->track, t->clip))
				goto failed;
		}
		else
			w->project = *p;
		w->sounds = sounds;
		w->device = t->device;
		w->looping = t->loop || t->pattern_mode;
		w->metronome = t->metronome;
		const MsSong* song = &w->project.song;
		w->begin = llround(ms_tick_frame(song, w->looping ? song->loop_start : 0));
		w->end = llround(ms_tick_frame(song, w->looping ? song->loop_end : ms_song_end(song)));
		if (w->end <= w->begin)
			goto failed;
		if (w->cursor < w->begin)
			w->cursor = w->begin;
		t->loop_first = w->begin;
		t->loop_last = w->end;
		t->looping = w->looping;
		playback_fill(w);
		if (w->failed)
			goto failed;
		w->thread = SDL_CreateThread(playback_worker, "music-audio", w);
		if (!w->thread)
			goto failed;
		SDL_PauseAudioDevice(t->device, 0);
	}
	SDL_LockMutex(w->mutex);
	w->metronome = t->metronome;
	t->cursor = w->cursor;
	memcpy(t->meters, w->meters, sizeof(t->meters));
	int done = w->done;
	SDL_UnlockMutex(w->mutex);
	if (done)
	{
		t->playing = 0;
		ms_transport_seek(t, ms_transport_position(t));
	}
	return;
failed:
	t->playing = 0;
	ms_transport_seek(t, t->cursor);
}
void ms_audition(MsTransport* t, const MsProject* p, const MsSounds* sounds, int drum, int pitch)
{
	if (!t->device || t->playing || drum >= MS_DRUMS || pitch < 0 || pitch > 127)
		return;
	ms_transport_seek(t, t->cursor);
	int count = drum >= 0 ? sounds->lengths[drum] : MS_RATE / 2;
	int a = drum >= 0 ? p->song.drum_asset[drum] : -1;
	if (a >= 0)
		count = p->asset[a].frames < MS_RATE * 2 ? p->asset[a].frames : MS_RATE * 2;
	const MsTrack* track = t->track >= 0 && t->track < p->song.tracks ? &p->song.track[t->track] : NULL;
	int instrument = track && track->kind == MS_SYNTH ? track->instrument : 0;
	float pan = drum >= 0 ? p->song.drum_pan[drum] : track ? track->pan : 0;
	float gain = p->song.master * (drum >= 0 ? p->song.drum_volume[drum] : (track ? track->volume : 1) * .70710678f);
	float left = gain * sqrtf((1 - pan) * .5f), right = gain * sqrtf((1 + pan) * .5f);
	double hz = 440 * pow(2, (pitch - 69) / 12.0);
	for (int pos = 0; pos < count; pos += BLOCK)
	{
		float data[BLOCK * 2];
		int n = count - pos;
		if (n > BLOCK)
			n = BLOCK;
		for (int i = 0; i < n; ++i)
		{
			float v = drum >= 0 ? (a >= 0 ? (p->asset[a].pcm[(pos + i) * 2] + p->asset[a].pcm[(pos + i) * 2 + 1]) * .5f : sounds->drums[drum][pos + i])
				: ms_instrument_sample(sounds, hz, instrument, pos + i, count);
			data[i * 2] = clamp(v * left, -1, 1);
			data[i * 2 + 1] = clamp(v * right, -1, 1);
		}
		if (SDL_QueueAudio(t->device, data, (Uint32)n * 8))
		{
			SDL_ClearQueuedAudio(t->device);
			return;
		}
	}
	SDL_PauseAudioDevice(t->device, 0);
}
