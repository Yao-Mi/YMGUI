#include "history.h"
#include <stdlib.h>
#include <string.h>
int st_history_init(StHistory* h, const StProject* p)
{
	h->states = calloc(ST_UNDO + 1, sizeof(*p));
	if (!h->states)
		return 0;
	st_history_reset(h, p);
	return 1;
}
void st_history_free(StHistory* h)
{
	free(h->states);
	h->states = NULL;
}
void st_history_reset(StHistory* h, const StProject* p)
{
	h->cursor = 0;
	h->count = 1;
	h->states[0] = *p;
}
void st_history_push(StHistory* h, const StProject* p)
{
	if (h->cursor == ST_UNDO)
	{
		memmove(h->states, h->states + 1, ST_UNDO * sizeof(*p));
		h->cursor--;
	}
	h->states[++h->cursor] = *p;
	h->count = h->cursor + 1;
}
int st_history_step(StHistory* h, StProject* p, int d)
{
	if (d != -1 && d != 1)
		return 0;
	int n = h->cursor + d;
	if (n < 0 || n >= h->count)
		return 0;
	h->cursor = n;
	*p = h->states[n];
	return 1;
}
