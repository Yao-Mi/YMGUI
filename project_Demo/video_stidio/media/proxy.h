#ifndef ST_PROXY_H
#define ST_PROXY_H
#include <stddef.h>
typedef struct StProxy StProxy;
StProxy* st_proxy_create(void);
void st_proxy_destroy(StProxy* p);
int st_proxy_queue(StProxy* p, const char* source);
/* 0 absent, 1 queued/building, 2 ready, -1 failed. Copies ready path under lock. */
int st_proxy_lookup(StProxy* p, const char* source, char* path, size_t capacity);
#endif
