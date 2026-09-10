/* Copyright (c) 2026. Immutable edit snapshot -> source RGB frames -> H.264 MP4. */
#define _GNU_SOURCE
#include "export.h"
#include "decoder.h"
#include "model/render_plan.h"
#include <libavutil/time.h>
#include <pthread.h>
#include <stdatomic.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#ifndef STUDIO_PROXY_FFMPEG
#define STUDIO_PROXY_FFMPEG "ffmpeg"
#endif
#define EXPORT_W 1280
#define EXPORT_H 720
#define FRAME_BYTES ((size_t)EXPORT_W * EXPORT_H * 3)
extern char** environ;
struct StExport
{
	pthread_mutex_t mutex;
	pthread_t thread;
	int joinable;
	atomic_int cancel;
	StProject project;
	StExportStatus status;
	int existed;
	struct stat target;
};
static void status_message(StExport* j, StExportState state, const char* text)
{
	pthread_mutex_lock(&j->mutex);
	j->status.state = state;
	snprintf(j->status.message, sizeof(j->status.message), "%s", text);
	pthread_mutex_unlock(&j->mutex);
}
static int fail(StExport* j, const char* message)
{
	pthread_mutex_lock(&j->mutex);
	j->status.frames = 0;
	j->status.total = 0;
	pthread_mutex_unlock(&j->mutex);
	status_message(j, ST_EXPORT_FAILED, message);
	return 0;
}
static int write_frame(StExport* j, int fd, const uint8_t* pixels)
{
	size_t offset = 0;
	int64_t deadline = av_gettime_relative() + 30000000;
	while (offset < FRAME_BYTES && !atomic_load(&j->cancel))
	{
		ssize_t n = write(fd, pixels + offset, FRAME_BYTES - offset);
		if (n > 0)
		{
			offset += (size_t)n;
			deadline = av_gettime_relative() + 30000000;
			continue;
		}
		if (n < 0 && errno != EAGAIN && errno != EINTR)
			return 0;
		if (av_gettime_relative() > deadline)
			return 0;
		struct pollfd p = {fd, POLLOUT, 0};
		int r = poll(&p, 1, 50);
		if (r < 0 && errno != EINTR)
			return 0;
		if (r > 0 && (p.revents & (POLLERR | POLLHUP | POLLNVAL)))
			return 0;
	}
	return offset == FRAME_BYTES;
}
static int wait_encoder(StExport* j, pid_t child, int abort)
{
	int status = 0, terminated = 0, killed = 0;
	int64_t deadline = av_gettime_relative() + 30000000, kill_at = 0;
	for (;;)
	{
		pid_t done = waitpid(child, &status, WNOHANG);
		if (done == child)
			return !terminated && WIFEXITED(status) && WEXITSTATUS(status) == 0;
		if (done < 0 && errno != EINTR)
			return 0;
		int64_t now = av_gettime_relative();
		if (!terminated && (abort || atomic_load(&j->cancel) || now > deadline))
		{
			kill(child, SIGTERM);
			terminated = 1;
			kill_at = now + 1000000;
		}
		if (terminated && !killed && now > kill_at)
		{
			kill(child, SIGKILL);
			killed = 1;
		}
		struct timespec delay = {0, 10000000};
		nanosleep(&delay, NULL);
	}
}
static int unchanged(StExport* j)
{
	struct stat now;
	if (lstat(j->status.path, &now))
		return !j->existed && errno == ENOENT;
	return j->existed && S_ISREG(now.st_mode) && now.st_dev == j->target.st_dev && now.st_ino == j->target.st_ino && now.st_size == j->target.st_size && now.st_mtim.tv_sec == j->target.st_mtim.tv_sec && now.st_mtim.tv_nsec == j->target.st_mtim.tv_nsec && now.st_ctim.tv_sec == j->target.st_ctim.tv_sec && now.st_ctim.tv_nsec == j->target.st_ctim.tv_nsec;
}
static void error_tail(int fd, char* message, size_t cap)
{
	off_t end = lseek(fd, 0, SEEK_END);
	if (end <= 0)
		return;
	off_t start = end > (off_t)(cap - 1) ? end - (off_t)(cap - 1) : 0;
	if (lseek(fd, start, SEEK_SET) < 0)
		return;
	ssize_t n = read(fd, message, cap - 1);
	if (n > 0)
	{
		message[n] = 0;
		for (ssize_t i = 0; i < n; i++)
			if (message[i] == '\n' || message[i] == '\r')
				message[i] = ' ';
	}
}
static void* worker(void* user)
{
	StExport* j = user;
	StRenderPlan* plan = malloc(sizeof(*plan));
	uint8_t* pixels = malloc(FRAME_BYTES);
	StDecoder decoder = {0};
	StDecodeControl control = {.stop = &j->cancel};
	char temp[1100] = "", log[1100] = "", message[256] = "导出失败";
	int fd = -1, logfd = -1, pipefd[2] = {-1, -1}, ok = 0;
	pid_t child = -1;
	sigset_t blocked;
	sigemptyset(&blocked);
	sigaddset(&blocked, SIGPIPE);
	pthread_sigmask(SIG_BLOCK, &blocked, NULL);
	if (!plan || !pixels || !st_render_plan(&j->project, plan))
	{
		strcpy(message, "无法创建渲染计划");
		goto done;
	}
	if (atomic_load(&j->cancel))
		goto done;
	snprintf(temp, sizeof(temp), "%s.partial-XXXXXX", j->status.path);
	fd = mkstemp(temp);
	if (fd < 0)
	{
		temp[0] = 0;
		strcpy(message, "无法创建导出临时文件");
		goto done;
	}
	close(fd);
	fd = -1;
	snprintf(log, sizeof(log), "%s.log-XXXXXX", j->status.path);
	logfd = mkstemp(log);
	if (logfd < 0)
	{
		log[0] = 0;
		strcpy(message, "无法创建编码日志");
		goto done;
	}
	if (fcntl(logfd, F_SETFD, FD_CLOEXEC) < 0)
		goto done;
	if (pipe2(pipefd, O_CLOEXEC))
	{
		strcpy(message, "无法创建编码管道");
		goto done;
	}
	posix_spawn_file_actions_t actions;
	if (posix_spawn_file_actions_init(&actions))
		goto done;
	int r = posix_spawn_file_actions_adddup2(&actions, pipefd[0], STDIN_FILENO);
	if (!r)
		r = posix_spawn_file_actions_adddup2(&actions, logfd, STDERR_FILENO);
	if (!r)
		r = posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
	if (!r)
		r = posix_spawn_file_actions_addclose(&actions, pipefd[0]);
	if (!r)
		r = posix_spawn_file_actions_addclose(&actions, pipefd[1]);
	if (!r)
		r = posix_spawn_file_actions_addclose(&actions, logfd);
	char* args[] = {STUDIO_PROXY_FFMPEG, "-nostdin", "-v", "error", "-y", "-f", "rawvideo", "-pixel_format", "rgb24", "-video_size", "1280x720", "-framerate", "25", "-i", "pipe:0",
					"-an", "-vf", "scale=in_range=full:out_range=tv:out_color_matrix=bt709", "-c:v", "libx264", "-preset", "veryfast", "-crf", "18", "-threads", "2", "-pix_fmt", "yuv420p",
					"-color_primaries", "bt709", "-color_trc", "bt709", "-colorspace", "bt709", "-movflags", "+faststart", "-f", "mp4", temp, NULL};
	if (!r)
		r = posix_spawnp(&child, STUDIO_PROXY_FFMPEG, &actions, NULL, args, environ);
	posix_spawn_file_actions_destroy(&actions);
	close(pipefd[0]);
	pipefd[0] = -1;
	if (r)
	{
		snprintf(message, sizeof(message), "无法启动编码器：%s", strerror(r));
		child = -1;
		goto done;
	}
	if (fcntl(pipefd[1], F_SETFL, fcntl(pipefd[1], F_GETFL) | O_NONBLOCK) < 0)
		goto done;
	for (int span_index = 0; span_index < plan->count; span_index++)
	{
		StRenderSpan span = plan->spans[span_index];
		if (atomic_load(&j->cancel))
			goto done;
		if (span.media >= 0 && strcmp(decoder.path, j->project.media[span.media].path))
		{
			control.deadline = av_gettime_relative() + 10000000;
			if ((r = st_decoder_open(&control, &decoder, j->project.media[span.media].path)) < 0)
			{
				char error[96];
				av_strerror(r, error, sizeof(error));
				snprintf(message, sizeof(message), "原素材无法打开：%s", error);
				goto done;
			}
		}
		for (int i = 0; i < span.length; i++)
		{
			if (atomic_load(&j->cancel))
				goto done;
			if (span.media < 0)
				memset(pixels, 0, FRAME_BYTES);
			else
			{
				control.deadline = av_gettime_relative() + 10000000;
				if ((r = st_decoder_read(&control, &decoder, span.in + i, pixels, EXPORT_W, EXPORT_H, AV_PIX_FMT_RGB24)) < 0)
				{
					char error[96];
					av_strerror(r, error, sizeof(error));
					snprintf(message, sizeof(message), "读取原素材失败：%s", error);
					goto done;
				}
			}
			if (!write_frame(j, pipefd[1], pixels))
			{
				strcpy(message, "编码器停止接收画面");
				goto done;
			}
			pthread_mutex_lock(&j->mutex);
			j->status.frames = span.start + i + 1;
			pthread_mutex_unlock(&j->mutex);
		}
	}
	close(pipefd[1]);
	pipefd[1] = -1;
	r = wait_encoder(j, child, 0);
	child = -1;
	if (!r)
	{
		strcpy(message, "编码器未能完成输出");
		error_tail(logfd, message, sizeof(message));
		goto done;
	}
	if (atomic_load(&j->cancel))
		goto done;
	fd = open(temp, O_RDONLY);
	if (fd < 0 || fsync(fd))
	{
		strcpy(message, "输出文件写入失败");
		goto done;
	}
	close(fd);
	fd = -1;
	if (!unchanged(j))
	{
		strcpy(message, "目标文件在导出期间发生变化，未覆盖");
		goto done;
	}
	/* link() publishes new destinations without ever replacing a concurrently created file. */
	r = j->existed ? rename(temp, j->status.path) : link(temp, j->status.path);
	if (r)
	{
		strcpy(message, "无法发布输出文件");
		goto done;
	}
	ok = 1;
done:
	if (pipefd[0] >= 0)
		close(pipefd[0]);
	if (pipefd[1] >= 0)
		close(pipefd[1]);
	if (child > 0)
	{
		wait_encoder(j, child, 1);
		if (!strncmp(message, "编码器", 9))
			error_tail(logfd, message, sizeof(message));
	}
	if (fd >= 0)
		close(fd);
	if (logfd >= 0)
		close(logfd);
	if (temp[0])
		unlink(temp);
	if (log[0])
		unlink(log);
	st_decoder_close(&decoder);
	free(plan);
	free(pixels);
	if (ok)
		status_message(j, ST_EXPORT_DONE, "导出完成");
	else if (atomic_load(&j->cancel))
		status_message(j, ST_EXPORT_CANCELLED, "导出已取消");
	else
		status_message(j, ST_EXPORT_FAILED, message);
	return NULL;
}
StExport* st_export_create(void)
{
	StExport* j = calloc(1, sizeof(*j));
	if (!j)
		return NULL;
	atomic_init(&j->cancel, 0);
	if (pthread_mutex_init(&j->mutex, NULL))
	{
		free(j);
		return NULL;
	}
	return j;
}
void st_export_cancel(StExport* j)
{
	if (j)
		atomic_store(&j->cancel, 1);
}
void st_export_destroy(StExport* j)
{
	if (!j)
		return;
	st_export_cancel(j);
	if (j->joinable)
		pthread_join(j->thread, NULL);
	pthread_mutex_destroy(&j->mutex);
	free(j);
}
void st_export_status(StExport* j, StExportStatus* out)
{
	pthread_mutex_lock(&j->mutex);
	*out = j->status;
	pthread_mutex_unlock(&j->mutex);
}
int st_export_start(StExport* j, const StProject* p, const char* path, int overwrite)
{
	StExportStatus current;
	st_export_status(j, &current);
	if (current.state == ST_EXPORT_RUNNING)
		return 0;
	if (j->joinable)
	{
		pthread_join(j->thread, NULL);
		j->joinable = 0;
	}
	if (!st_project_valid(p) || !st_duration(p))
		return fail(j, "项目中没有可导出的视频时间线");
	if (!path || path[0] != '/' || strlen(path) >= 1024 || strlen(path) < 5 || strcasecmp(path + strlen(path) - 4, ".mp4"))
		return fail(j, "请选择绝对路径，文件名使用 .mp4 扩展名");
	struct stat target = {0};
	j->existed = lstat(path, &target) == 0;
	if (!j->existed && errno != ENOENT)
		return fail(j, "无法访问目标路径");
	if (j->existed && (!overwrite || !S_ISREG(target.st_mode)))
		return fail(j, "目标文件已存在或不是普通文件");
	for (int i = 0; i < p->media_count; i++)
	{
		struct stat source;
		if (!strcmp(path, p->media[i].path) || (j->existed && !stat(p->media[i].path, &source) && source.st_dev == target.st_dev && source.st_ino == target.st_ino))
			return fail(j, "不能用导出文件覆盖原素材");
	}
	j->target = target;
	j->project = *p;
	atomic_store(&j->cancel, 0);
	pthread_mutex_lock(&j->mutex);
	j->status = (StExportStatus){.state = ST_EXPORT_RUNNING, .total = st_duration(p)};
	snprintf(j->status.path, sizeof(j->status.path), "%s", path);
	strcpy(j->status.message, "正在导出 MP4");
	pthread_mutex_unlock(&j->mutex);
	if (pthread_create(&j->thread, NULL, worker, j))
		return fail(j, "无法创建导出任务");
	j->joinable = 1;
	return 1;
}
