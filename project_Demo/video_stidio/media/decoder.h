#ifndef ST_DECODER_H
#define ST_DECODER_H
#include <stdatomic.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
typedef struct
{
	atomic_int* stop;
	int64_t deadline;
} StDecodeControl;
typedef struct
{
	char path[1024];
	AVFormatContext* fmt;
	AVCodecContext* codec;
	AVFrame* frame;
	AVFrame* scratch;
	AVPacket* packet;
	struct SwsContext* scale;
	int stream, eof;
	int64_t last_us, origin;
	uint64_t used;
} StDecoder;
void st_decoder_close(StDecoder* d);
int st_decoder_open(StDecodeControl* control, StDecoder* d, const char* path);
int st_decoder_read(StDecodeControl* control, StDecoder* d, int frame, uint8_t* pixels, int width, int height, enum AVPixelFormat output);
#endif
