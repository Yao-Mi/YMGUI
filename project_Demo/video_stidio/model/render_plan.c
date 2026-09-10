#include "render_plan.h"
#include <stdlib.h>
#include <string.h>
static int compare(const void* a, const void* b)
{
	int x = *(const int*)a, y = *(const int*)b;
	return (x > y) - (x < y);
}
int st_render_plan(const StProject* p, StRenderPlan* plan)
{
	if (!st_project_valid(p))
		return 0;
	memset(plan, 0, sizeof(*plan));
	int edges[ST_RENDER_SPANS + 1], n = 0;
	edges[n++] = 0;
	plan->frames = st_duration(p);
	if (!plan->frames)
		return 0;
	for (int t = 0; t < p->track_count; t++)
		for (int i = 0; i < p->tracks[t].count; i++)
		{
			StClip c = p->tracks[t].clips[i];
			edges[n++] = c.start;
			edges[n++] = c.start + c.length;
		}
	qsort(edges, n, sizeof(int), compare);
	for (int i = 0; i < n - 1; i++)
	{
		int start = edges[i], length = edges[i + 1] - start;
		if (!length)
			continue;
		const StClip* c = st_visible(p, start);
		StRenderSpan span = {start, length, c ? c->media : -1, c ? c->in + start - c->start : 0};
		StRenderSpan* last = plan->count ? &plan->spans[plan->count - 1] : NULL;
		if (last && last->media == span.media && (span.media < 0 || last->in + last->length == span.in))
			last->length += length;
		else
			plan->spans[plan->count++] = span;
	}
	return plan->count > 0;
}
