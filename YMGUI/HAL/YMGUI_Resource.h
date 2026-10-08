#ifndef YMGUI_RESOURCE_H
#define YMGUI_RESOURCE_H
#include "YMGUI_PubType.h"
#include <stddef.h>
typedef struct
{
	void* user;
	void* (*open)(const char* path, size_t* size, void* user);
	size_t (*read_at)(void* file, size_t offset, void* out, size_t size, void* user);
	void (*close)(void* file, void* user);
} GYresource_io;
/* Backend is copied. File handles retain their own backend until Close. */
typedef struct
{
	void* handle;
	size_t size;
	GYresource_io io;
} GYresource;
void YMGUI_Resource_SetBackend(const GYresource_io* io);
uint8 YMGUI_Resource_Open(GYresource* resource, const char* path);
size_t YMGUI_Resource_Read(GYresource* resource, size_t offset, void* out, size_t size);
void YMGUI_Resource_Close(GYresource* resource);
/* Returned whole-file data belongs to memory region 1. */
void* YMGUI_Resource_Load(const char* path, size_t* size);
#endif
