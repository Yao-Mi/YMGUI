#include "YMGUI_PubDefine.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Mem.h"
#include <stdio.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    test_async.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-02
  *	@Description: 异步双缓冲回归。单线程桌面模拟裸机 DMA 时序:
  *	              flush_cb 只登记"这块开始传了"(inflight++),不立刻完成 → 模拟 DMA 在跑;
  *	              wait_cb 里调 FlushReady + inflight-- → 模拟 DMA 传输完成中断。
  *	              验证:①buffer 在 buf1/buf2 间 ping-pong 交替;②任一时刻在途传输 <=1
  *	              (库发起下块前必等上块传完);③buf2=NULL 单缓冲退化路径覆盖完整、不触发等待逻辑。
  ***************************************************************************************************************************/

#define SCR_W 320
#define SCR_H 240

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); fails++; } } while (0)

//—— 双缓冲 mock 状态 ——
static GYdisp*      g_disp;
static int          g_inflight;      //当前在途传输数
static int          g_max_inflight;  //峰值(应恒 <=1)
static int          g_flush_cnt;     //flush 次数
static const GYpx*  g_last_buf;      //上次 flush 的 buffer 指针
static int          g_alt_ok;        //相邻两次 flush 是否总换了 buffer
static long         g_total_px;      //累计 flush 面积

//异步 flush:登记开始传输,不立刻完成(FlushReady 留给 wait_cb 模拟的 DMA 中断去调)
static void asyncFlush(GYdisp* d, const GYrect* a, const GYpx* b)
{
	(void)d;
	g_inflight++;
	if (g_inflight > g_max_inflight)
		g_max_inflight = g_inflight;
	//相邻两次 flush 的 buffer 必须不同(ping-pong)
	if (g_flush_cnt > 0 && b == g_last_buf)
		g_alt_ok = 0;
	g_last_buf = b;
	g_flush_cnt++;
	g_total_px += (long)a->w * a->h;
}

//模拟 DMA 完成中断:清 busy + 结束一笔在途传输
static void asyncWait(GYdisp* d)
{
	g_inflight--;
	YMGUI_Disp_FlushReady(d);
}

//—— 单缓冲 mock ——
static long g_sync_px;
static void syncFlush(GYdisp* d, const GYrect* a, const GYpx* b)
{
	(void)b;
	g_sync_px += (long)a->w * a->h;
	YMGUI_Disp_FlushReady(d);//同步 port:传完即就绪
}

static void resetAsync(void)
{
	g_inflight = 0; g_max_inflight = 0; g_flush_cnt = 0;
	g_last_buf = NULL; g_alt_ok = 1; g_total_px = 0;
}

int main(void)
{
	//—— 双缓冲:buf 各 100 像素,全屏脏 → 多 band,逼出 ping-pong ——
	uint32 buf_px = 100;
	GYdisp disp;
	disp.hor_res = SCR_W; disp.ver_res = SCR_H;
	disp.buf_px_cnt = buf_px;
	disp.buf1 = (GYpx*)GY_malloc1(buf_px * sizeof(GYpx));
	disp.buf2 = (GYpx*)GY_malloc1(buf_px * sizeof(GYpx));
	disp.flush_cb = asyncFlush;
	disp.wait_cb = asyncWait;
	disp.flush_busy = 0;
	disp.user_data = NULL;
	g_disp = &disp;

	GYCTX ctx = YMGUI_Creat_Ctx_Creat(&disp, SCR_W, SCR_H);
	YMGUI_Obj_SetBgColor(ctx->root, GY_ARGB(0xFF, 0x40, 0x40, 0x40));
	YMGUI_Refresh(ctx);//刷掉初始全屏脏

	resetAsync();
	GYrect full = {0, 0, SCR_W, SCR_H};
	YMGUI_Ctx_InvalidateArea(ctx, &full);
	YMGUI_Refresh(ctx);

	CHECK(g_flush_cnt > 1, "double-buffer full screen produces multiple bands");
	CHECK(g_alt_ok, "buffer ping-pongs (adjacent flushes use different buffers)");
	CHECK(g_max_inflight <= 1, "at most 1 transfer in flight at any time");
	CHECK(g_inflight == 0, "all transfers drained when Refresh returns");
	CHECK(disp.flush_busy == 0, "flush_busy cleared after Refresh");
	CHECK(g_total_px == (long)SCR_W * SCR_H, "double-buffer coverage complete (no gap/overlap)");

	YMGUI_Free_CtxFree(ctx);
	GY_free1(disp.buf1);
	GY_free1(disp.buf2);

	//—— 单缓冲退化:buf2=NULL,不该触发任何等待逻辑,覆盖仍完整 ——
	GYdisp d2;
	d2.hor_res = SCR_W; d2.ver_res = SCR_H;
	d2.buf_px_cnt = buf_px;
	d2.buf1 = (GYpx*)GY_malloc1(buf_px * sizeof(GYpx));
	d2.buf2 = NULL;
	d2.flush_cb = syncFlush;
	d2.wait_cb = NULL;
	d2.flush_busy = 0;
	d2.user_data = NULL;

	GYCTX ctx2 = YMGUI_Creat_Ctx_Creat(&d2, SCR_W, SCR_H);
	YMGUI_Obj_SetBgColor(ctx2->root, GY_ARGB(0xFF, 0x20, 0x20, 0x20));
	YMGUI_Refresh(ctx2);
	g_sync_px = 0;
	YMGUI_Ctx_InvalidateArea(ctx2, &full);
	YMGUI_Refresh(ctx2);
	CHECK(g_sync_px == (long)SCR_W * SCR_H, "single-buffer path coverage complete");

	YMGUI_Free_CtxFree(ctx2);
	GY_free1(d2.buf1);

	if (fails == 0)
		printf("test_async: ALL PASS\n");
	else
		printf("test_async: %d FAILED\n", fails);
	return fails ? 1 : 0;
}
