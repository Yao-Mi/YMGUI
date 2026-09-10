#ifndef ST_HISTORY_H
#define ST_HISTORY_H
#include "project.h"
#define ST_UNDO 32
typedef struct
{
	StProject* states;
	int cursor, count;
} StHistory;
int st_history_init(StHistory* h, const StProject* p);
void st_history_free(StHistory* h);
void st_history_reset(StHistory* h, const StProject* p);
void st_history_push(StHistory* h, const StProject* p);
int st_history_step(StHistory* h, StProject* p, int delta);
#endif
