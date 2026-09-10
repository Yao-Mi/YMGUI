#ifndef ST_ENGINE_H
#define ST_ENGINE_H
#include "model/project.h"
#include <stdint.h>
#define ST_PREVIEW_W 640
#define ST_PREVIEW_H 360
#define ST_PIXELS (ST_PREVIEW_W * ST_PREVIEW_H)
typedef struct StEngine StEngine;
typedef struct
{
	StMedia media;
	int ok;
	char error[128];
	uint16_t thumb[160 * 90];
} StImportResult;
typedef struct
{
	uint64_t serial;
	int frame, ok;
	double elapsed_ms;
	char error[128];
} StFrameResult;
StEngine* st_engine_create(void);
void st_engine_destroy(StEngine* e);
int st_engine_proxy_state(StEngine* e, const char* source);
int st_engine_import(StEngine* e, const char* path);
int st_engine_poll_import(StEngine* e, StImportResult* out);
uint64_t st_engine_request(StEngine* e, const char* path, int frame);
int st_engine_poll_frame(StEngine* e, uint16_t* pixels, StFrameResult* out);
#define ST_THUMB_W 80
#define ST_THUMB_H 45
typedef struct
{
	char path[1024];
	int frame, ok;
	uint16_t pixels[ST_THUMB_W * ST_THUMB_H];
} StThumbnailResult;
int st_engine_thumbnail(StEngine* e, const char* path, int frame);
int st_engine_poll_thumbnail(StEngine* e, StThumbnailResult* result);
#endif
