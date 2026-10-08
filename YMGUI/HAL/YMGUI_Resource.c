#include "YMGUI_Resource.h"
#include "YMGUI_Mem.h"
#ifndef YMGUI_RESOURCE_STDIO
#define YMGUI_RESOURCE_STDIO 1
#endif
#if YMGUI_RESOURCE_STDIO
#include <stdio.h>
#include <limits.h>
static void* stdOpen(const char* path, size_t* size, void* user)
{
	(void)user;
	FILE* f = fopen(path, "rb");
	if (!f)
		return NULL;
	if (fseek(f, 0, SEEK_END))
	{
		fclose(f);
		return NULL;
	}
	long n = ftell(f);
	if (n < 0)
	{
		fclose(f);
		return NULL;
	}
	*size = (size_t)n;
	return f;
}
static size_t stdRead(void* file, size_t offset, void* out, size_t size, void* user)
{
	(void)user;
	if (offset > LONG_MAX || fseek(file, (long)offset, SEEK_SET))
		return 0;
	return fread(out, 1, size, file);
}
static void stdClose(void* file, void* user)
{
	(void)user;
	fclose(file);
}
#endif
static GYresource_io backend;
void YMGUI_Resource_SetBackend(const GYresource_io* io)
{
	if (io)
		backend = *io;
	else
		GY_memset(&backend, 0, sizeof(backend));
}
uint8 YMGUI_Resource_Open(GYresource* resource, const char* path)
{
	if (!resource || !path)
		return 0;
	GY_memset(resource, 0, sizeof(*resource));
	resource->io = backend;
#if YMGUI_RESOURCE_STDIO
	if (!resource->io.open)
		resource->io = (GYresource_io){NULL, stdOpen, stdRead, stdClose};
#endif
	if (!resource->io.open || !resource->io.read_at || !resource->io.close)
		return 0;
	resource->handle = resource->io.open(path, &resource->size, resource->io.user);
	return resource->handle != NULL;
}
size_t YMGUI_Resource_Read(GYresource* resource, size_t offset, void* out, size_t size)
{
	if (!resource || !resource->handle || !out || offset > resource->size || size > resource->size - offset)
		return 0;
	size_t read = resource->io.read_at(resource->handle, offset, out, size, resource->io.user);
	return read <= size ? read : 0;
}
void YMGUI_Resource_Close(GYresource* resource)
{
	if (!resource)
		return;
	if (resource->handle)
		resource->io.close(resource->handle, resource->io.user);
	GY_memset(resource, 0, sizeof(*resource));
}
void* YMGUI_Resource_Load(const char* path, size_t* size)
{
	GYresource file;
	if (size)
		*size = 0;
	if (!YMGUI_Resource_Open(&file, path))
		return NULL;
	void* data = file.size ? GY_malloc1(file.size) : NULL;
	if (!data || YMGUI_Resource_Read(&file, 0, data, file.size) != file.size)
	{
		GY_free1(data);
		data = NULL;
	}
	if (data && size)
		*size = file.size;
	YMGUI_Resource_Close(&file);
	return data;
}
