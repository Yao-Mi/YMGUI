#ifndef STUDIO_PROJECT_H
#define STUDIO_PROJECT_H
#include <stdint.h>
#define ST_MEDIA 64
#define ST_TRACKS 8
#define ST_CLIPS 128
#define ST_FPS 25
#define ST_MAX_FRAME (25 * 60 * 60 * 24) /* 24-hour project bound */
typedef struct
{
	char path[1024];
	int frames, width, height;
} StMedia;
typedef struct
{
	int id, media, start, in, length;
} StClip;
typedef struct
{
	int count, visible, locked;
	StClip clips[ST_CLIPS];
} StTrack;
typedef struct
{
	int media_count, track_count, next_id;
	StMedia media[ST_MEDIA];
	StTrack tracks[ST_TRACKS];
} StProject;
typedef enum
{
	ST_ADD,
	ST_MOVE,
	ST_TRIM,
	ST_SPLIT,
	ST_LIFT,
	ST_RIPPLE,
	ST_TRACK_ADD,
	ST_TRACK_HIDE,
	ST_TRACK_LOCK
} StEditKind;
typedef struct
{
	StEditKind kind;
	int track, id, media, at, in, length;
} StEdit;
void st_project_init(StProject* p);
int st_duration(const StProject* p);
const StClip* st_clip(const StProject* p, int id, int* track);
const StClip* st_visible(const StProject* p, int frame);
int st_project_valid(const StProject* p);
int st_project_edit(StProject* p, StEdit edit);
int st_project_save(const StProject* p, const char* path);
int st_project_load(StProject* p, const char* path);
const char* st_basename(const char* path);
#endif
