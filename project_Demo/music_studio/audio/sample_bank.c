#include "audio.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SAMPLE_FRAMES 144000
#define SAMPLE_RATE 24000

int ms_sample_bank_load(float** zones, const char* path)
{
	FILE* file = fopen(path, "rb");
	if (!file)
		return 0;
	unsigned char header[28];
	const unsigned values[] = {SAMPLE_RATE, MS_SAMPLE_ZONES, SAMPLE_FRAMES, 36, 4};
	int ok = fread(header, 1, sizeof(header), file) == sizeof(header) && !memcmp(header, "MSCHOIR1", 8);
	for (int field = 0; ok && field < 5; ++field)
	{
		unsigned value = 0;
		for (int byte = 0; byte < 4; ++byte)
			value |= (unsigned)header[8 + field * 4 + byte] << (byte * 8);
		ok = value == values[field];
	}
	for (int zone = 0; ok && zone < MS_SAMPLE_ZONES; ++zone)
	{
		zones[zone] = malloc(SAMPLE_FRAMES * sizeof(float));
		if (!zones[zone])
		{
			ok = 0;
			break;
		}
		for (int pos = 0; ok && pos < SAMPLE_FRAMES; pos += 1024)
		{
			unsigned char data[2048];
			int count = SAMPLE_FRAMES - pos;
			if (count > 1024)
				count = 1024;
			ok = fread(data, 2, count, file) == (size_t)count;
			if (!ok)
				break;
			for (int i = 0; i < count; ++i)
			{
				int value = data[i * 2] | ((unsigned)data[i * 2 + 1] << 8);
				if (value >= 32768)
					value -= 65536;
				zones[zone][pos + i] = value / 32768.f;
			}
		}
	}
	ok = ok && fgetc(file) == EOF && !ferror(file);
	fclose(file);
	if (!ok)
		for (int zone = 0; zone < MS_SAMPLE_ZONES; ++zone)
		{
			free(zones[zone]);
			zones[zone] = NULL;
		}
	return ok;
}

static float interpolate(const float* data, double position)
{
	int i = (int)position;
	float mix = (float)(position - i);
	return data[i] + (data[i + 1] - data[i]) * mix;
}

float ms_sample_bank_play(float* const* zones, double hz, int pos, int length, int loop)
{
	if (!isfinite(hz) || hz <= 0 || pos < 0 || pos >= length)
		return 0;
	double pitch = 69 + 12 * log2(hz / 440);
	int zone = (int)lround((pitch - 36) / 4);
	if (zone < 0)
		zone = 0;
	if (zone >= MS_SAMPLE_ZONES)
		zone = MS_SAMPLE_ZONES - 1;
	const float* data = zones[zone];
	if (!data)
		return 0; /* 缺失资源时不冒充人声回退到旧合成器。 */
	double ratio = hz / (440 * exp2((36 + zone * 4 - 69) / 12.0));
	double sample = pos * ratio * ((double)SAMPLE_RATE / MS_RATE);
	const double start = SAMPLE_RATE, end = SAMPLE_RATE * 5, fade = SAMPLE_RATE * .1;
	/* 起音只播放一次；100 ms 重叠交叉淡化后，从已混过的片头之后继续。 */
	if (!loop && sample >= SAMPLE_FRAMES - 1)
		return 0;
	if (loop && sample >= end)
		sample = start + fade + fmod(sample - end, end - start - fade);
	float value = interpolate(data, sample);
	if (!loop)
		value *= (float)fmin(1, (SAMPLE_FRAMES - 1 - sample) / (SAMPLE_RATE * .05));
	if (loop && sample >= end - fade)
	{
		float mix = (float)((sample - (end - fade)) / fade);
		value = value * (1 - mix) + interpolate(data, start + sample - (end - fade)) * mix;
	}
	float envelope = fminf(1, pos / (MS_RATE * .005f)) * fminf(1, (length - pos) / (MS_RATE * .15f));
	return value * .38f * envelope;
}

float ms_instrument_sample(const MsSounds* sounds, double hz, int instrument, int pos, int length)
{
	if (instrument < 0 || instrument >= MS_INSTRUMENTS)
		return 0;
	/* 击弦、拨弦及定音鼓保留自然衰减；持续音色使用采样内交叉循环。 */
	int loop = instrument != 0 && instrument != 1 && instrument != 2 && instrument != 3 && instrument != 10;
	return ms_sample_bank_play(sounds->instrument[instrument], hz, pos, length, loop);
}
