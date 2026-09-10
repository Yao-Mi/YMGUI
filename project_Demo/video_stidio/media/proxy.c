/* Copyright (c) 2026. Offline proxy jobs; scrubbing never starts a subprocess. */
#include "proxy.h"
#include "model/project.h"
#include <pthread.h>
#include <spawn.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#ifndef STUDIO_PROXY_FFMPEG
#define STUDIO_PROXY_FFMPEG "ffmpeg"
#endif
extern char** environ;
struct StProxy
{
	pthread_t worker;
	pthread_mutex_t mutex;
	pthread_cond_t wake;
	int stop, count;
	char directory[80];
	struct
	{
		char source[1024], output[128];
		int status;
	} jobs[ST_MEDIA];
};
static int transcode(StProxy* p, int index)
{
	char* args[] = {STUDIO_PROXY_FFMPEG, "-nostdin", "-v", "error", "-y", "-threads", "2", "-filter_threads", "1", "-i", p->jobs[index].source,
					"-map", "0:v:0", "-an", "-vf", "scale=640:360:force_original_aspect_ratio=decrease:force_divisible_by=2,pad=640:360:(ow-iw)/2:(oh-ih)/2,fps=25",
					"-c:v", "mjpeg", "-q:v", "5", "-threads", "2", p->jobs[index].output, NULL};
	posix_spawn_file_actions_t actions;
	if (posix_spawn_file_actions_init(&actions))
		return 0;
	int r = posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
	if (!r)
		r = posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
	if (!r)
		r = posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
	pid_t child = -1;
	if (!r)
		r = posix_spawnp(&child, STUDIO_PROXY_FFMPEG, &actions, NULL, args, environ);
	posix_spawn_file_actions_destroy(&actions);
	if (r)
		return 0;
	struct timespec delay = {0, 10000000};
	int status = 0, cancelled = 0;
	int64_t cancel_tick = 0;
	for (;;)
	{
		pid_t done = waitpid(child, &status, WNOHANG);
		if (done == child)
			return !cancelled && WIFEXITED(status) && WEXITSTATUS(status) == 0;
		if (done < 0 && errno != EINTR)
			return 0;
		pthread_mutex_lock(&p->mutex);
		int stop = p->stop;
		pthread_mutex_unlock(&p->mutex);
		if (stop && !cancelled)
		{
			kill(child, SIGTERM);
			cancelled = 1;
		}
		if (cancelled && ++cancel_tick == 100)
			kill(child, SIGKILL);
		nanosleep(&delay, NULL);
	}
}
static void* worker(void* data)
{
	StProxy* p = data;
	for (;;)
	{
		pthread_mutex_lock(&p->mutex);
		int i;
		for (;;)
		{
			for (i = 0; i < p->count; i++)
				if (p->jobs[i].status == 1)
					break;
			if (p->stop || i < p->count)
				break;
			pthread_cond_wait(&p->wake, &p->mutex);
		}
		if (p->stop)
		{
			pthread_mutex_unlock(&p->mutex);
			break;
		}
		p->jobs[i].status = 3;
		pthread_mutex_unlock(&p->mutex);
		int ok = transcode(p, i);
		if (!ok)
			unlink(p->jobs[i].output);
		pthread_mutex_lock(&p->mutex);
		p->jobs[i].status = ok ? 2 : -1;
		pthread_mutex_unlock(&p->mutex);
	}
	return NULL;
}
StProxy* st_proxy_create(void)
{
	StProxy* p = calloc(1, sizeof(*p));
	if (!p)
		return NULL;
	strcpy(p->directory, "/tmp/ymstudio-preview-XXXXXX");
	if (!mkdtemp(p->directory))
	{
		free(p);
		return NULL;
	}
	if (pthread_mutex_init(&p->mutex, NULL))
		goto fail;
	if (pthread_cond_init(&p->wake, NULL))
	{
		pthread_mutex_destroy(&p->mutex);
		goto fail;
	}
	if (pthread_create(&p->worker, NULL, worker, p))
	{
		pthread_cond_destroy(&p->wake);
		pthread_mutex_destroy(&p->mutex);
		goto fail;
	}
	return p;
fail:
	rmdir(p->directory);
	free(p);
	return NULL;
}
void st_proxy_destroy(StProxy* p)
{
	if (!p)
		return;
	pthread_mutex_lock(&p->mutex);
	p->stop = 1;
	pthread_cond_signal(&p->wake);
	pthread_mutex_unlock(&p->mutex);
	pthread_join(p->worker, NULL);
	for (int i = 0; i < p->count; i++)
		unlink(p->jobs[i].output);
	rmdir(p->directory);
	pthread_cond_destroy(&p->wake);
	pthread_mutex_destroy(&p->mutex);
	free(p);
}
int st_proxy_queue(StProxy* p, const char* path)
{
	if (!p || strlen(path) >= 1024)
		return 0;
	pthread_mutex_lock(&p->mutex);
	int i;
	for (i = 0; i < p->count; i++)
		if (!strcmp(path, p->jobs[i].source))
			break;
	int ok = i < ST_MEDIA;
	if (ok && i == p->count)
	{
		strcpy(p->jobs[i].source, path);
		snprintf(p->jobs[i].output, sizeof(p->jobs[i].output), "%s/%d.avi", p->directory, i);
		p->jobs[i].status = 1;
		p->count++;
		pthread_cond_signal(&p->wake);
	}
	pthread_mutex_unlock(&p->mutex);
	return ok;
}
int st_proxy_lookup(StProxy* p, const char* source, char* path, size_t capacity)
{
	if (!p)
		return -1;
	pthread_mutex_lock(&p->mutex);
	int status = 0;
	for (int i = 0; i < p->count; i++)
		if (!strcmp(source, p->jobs[i].source))
		{
			status = p->jobs[i].status;
			if (status == 2 && path && capacity)
				snprintf(path, capacity, "%s", p->jobs[i].output);
			break;
		}
	pthread_mutex_unlock(&p->mutex);
	return status == 3 ? 1 : status;
}
