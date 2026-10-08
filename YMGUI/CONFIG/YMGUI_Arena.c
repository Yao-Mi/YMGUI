#include "YMGUI_Arena.h"
#include <stdint.h>
#include <string.h>
typedef struct Block
{
	size_t size;
	struct Block* next;
	size_t used, reserved;
} Block;
#define ALIGNMENT (sizeof(Block) > 8 ? sizeof(Block) : 8)
static size_t align_size(size_t n)
{
	size_t r = n % ALIGNMENT;
	return r ? n + ALIGNMENT - r : n;
}
int YMGUI_Arena_Init(GYarena* a, void* storage, size_t size)
{
	if (!a)
		return 0;
	memset(a, 0, sizeof(*a));
	size_t skip = storage ? (ALIGNMENT - (uintptr_t)storage % ALIGNMENT) % ALIGNMENT : 0;
	if (!storage || size < skip || size - skip < sizeof(Block) + ALIGNMENT)
		return 0;
	Block* b = (Block*)((unsigned char*)storage + skip);
	b->size = (size - skip - sizeof(Block)) / ALIGNMENT * ALIGNMENT;
	b->next = NULL;
	b->used = 0;
	a->head = b;
	return 1;
}
void* YMGUI_Arena_Alloc(GYarena* a, size_t n)
{
	if (!a || !n || n > (size_t)-1 - ALIGNMENT)
		return NULL;
	n = align_size(n);
	for (Block* b = a->head; b; b = b->next)
		if (!b->used && b->size >= n)
		{
			if (b->size - n >= sizeof(Block) + ALIGNMENT)
			{
				Block* t = (Block*)((unsigned char*)(b + 1) + n);
				t->size = b->size - n - sizeof(Block);
				t->used = 0;
				t->next = b->next;
				b->next = t;
				b->size = n;
			}
			b->used = 1;
			a->used += b->size;
			if (a->used > a->peak)
				a->peak = a->used;
			return b + 1;
		}
	return NULL;
}
void YMGUI_Arena_Free(GYarena* a, void* p)
{
	if (!a || !p)
		return;
	Block* b = a->head;
	while (b && b + 1 != p)
		b = b->next;
	if (!b || !b->used)
		return;
	a->used -= b->size;
	b->used = 0;
	for (b = a->head; b && b->next;)
	{
		if (!b->used && !b->next->used)
		{
			b->size += sizeof(Block) + b->next->size;
			b->next = b->next->next;
		}
		else
			b = b->next;
	}
}
void* YMGUI_Arena_Realloc(GYarena* a, void* p, size_t n)
{
	if (!p)
		return YMGUI_Arena_Alloc(a, n);
	if (!n)
	{
		YMGUI_Arena_Free(a, p);
		return NULL;
	}
	if (!a)
		return NULL;
	Block* b = a->head;
	while (b && b + 1 != p)
		b = b->next;
	if (!b || !b->used)
		return NULL;
	if (n <= b->size)
		return p;
	void* q = YMGUI_Arena_Alloc(a, n);
	if (q)
	{
		memcpy(q, p, b->size);
		YMGUI_Arena_Free(a, p);
	}
	return q;
}
size_t YMGUI_Arena_LargestFree(const GYarena* a)
{
	size_t largest = 0;
	if (a)
		for (Block* b = a->head; b; b = b->next)
			if (!b->used && b->size > largest)
				largest = b->size;
	return largest;
}
