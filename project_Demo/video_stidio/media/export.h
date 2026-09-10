#ifndef ST_EXPORT_H
#define ST_EXPORT_H
#include "model/project.h"
typedef struct StExport StExport;
typedef enum
{
	ST_EXPORT_IDLE,
	ST_EXPORT_RUNNING,
	ST_EXPORT_DONE,
	ST_EXPORT_CANCELLED,
	ST_EXPORT_FAILED
} StExportState;
typedef struct
{
	StExportState state;
	int frames, total;
	char path[1024], message[256];
} StExportStatus;
StExport* st_export_create(void);
void st_export_destroy(StExport* job);
/* Main-thread APIs. One active job. Snapshot copied before returning. */
int st_export_start(StExport* job, const StProject* project, const char* path, int overwrite);
void st_export_cancel(StExport* job);
void st_export_status(StExport* job, StExportStatus* status);
#endif
