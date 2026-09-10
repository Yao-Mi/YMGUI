#ifndef ST_RENDER_PLAN_H
#define ST_RENDER_PLAN_H
#include "project.h"
#define ST_RENDER_SPANS (ST_TRACKS * ST_CLIPS * 2 + 1)
typedef struct
{
	int start, length, media, in;
} StRenderSpan;
typedef struct
{
	int count, frames;
	StRenderSpan spans[ST_RENDER_SPANS];
} StRenderPlan;
int st_render_plan(const StProject* project, StRenderPlan* plan);
#endif
