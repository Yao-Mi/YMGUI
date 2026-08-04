#include "mp_spectrum.h"
#include "YMGUI_Trig.h"
#include "YMGUI_PubDefine.h"
#include <stdint.h>

typedef uint64_t uint64;

/**
  ***************************************************************************************************************************
  *	@FileName:    mp_spectrum.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-04
  *	@Description: 频段能量分析(app 侧)。对一窗 mono PCM 做整数 Goertzel(按二次分布近似对数的中心
  *	              频率),取幅度归一到柱值。无 libm:cos 用库 Q15 三角表,幅度用整数 isqrt。目标是
  *	              "能动、跟音乐相关",不追真 FFT 精度(库核心是 GUI 不是音视频分析)。
  *	@Version:     1.0
  ***************************************************************************************************************************/

//整数平方根(uint64 → uint32),逐位法,无 FPU
static uint32 isqrt64(uint64 v)
{
	uint64 res = 0;
	uint64 bit = (uint64)1 << 62;
	while (bit > v) bit >>= 2;
	while (bit != 0)
	{
		if (v >= res + bit)
		{
			v -= res + bit;
			res = (res >> 1) + bit;
		}
		else
		{
			res >>= 1;
		}
		bit >>= 2;
	}
	return (uint32)res;
}

//单频段 Goertzel 幅度(定点 coeff = 2*cos(w),Q15;w=360*f/rate 度)
static uint32 goertzelMag(const int16* x, int32 n, int32 deg)
{
	int32 c = GY_Cos(deg);            //Q15,[-32768,32767]
	int64 coeff = (int64)2 * c;       //Q15 的 2*cos,范围约 [-65536,65536]
	int64 s0, s1 = 0, s2 = 0;
	int32 i;
	for (i = 0; i < n; i++)
	{
		s0 = (int64)x[i] + ((coeff * s1) >> 15) - s2;
		s2 = s1;
		s1 = s0;
	}
	//power = s1^2 + s2^2 - coeff*s1*s2
	int64 power = s1 * s1 + s2 * s2 - ((coeff * s1) >> 15) * s2;
	if (power < 0) power = 0;
	return isqrt64((uint64)power);
}

void mp_spectrum_analyze(const int16* samples, int32 n, int32 rate,
                         int32 nbands, int32* out, int32 out_max)
{
	if (samples == NULL || out == NULL || n <= 0 || rate <= 0)
		return;
	nbands = GYLimitMaxMin(1, nbands, MP_SPEC_MAX_BANDS);
	if (out_max <= 0) out_max = 100;

	//中心频率:二次分布从 f_lo 到 f_hi(近似对数——低频段密,高频段疏)
	int32 f_lo = 60;
	int32 f_hi = rate / 2;
	if (f_hi > 16000) f_hi = 16000;
	if (f_hi <= f_lo) f_hi = f_lo + 100;

	int32 b;
	for (b = 0; b < nbands; b++)
	{
		//t = b/(nbands-1) 的二次:f = f_lo + (f_hi-f_lo)*t^2
		int32 denom = (nbands > 1) ? (nbands - 1) : 1;
		int64 t2num = (int64)b * b;                 //b^2
		int64 t2den = (int64)denom * denom;         //(nbands-1)^2
		int32 f = f_lo + (int32)(((int64)(f_hi - f_lo) * t2num) / t2den);
		if (f < 1) f = 1;
		int32 deg = (int32)(((int64)360 * f) / rate) % 360;

		uint32 mag = goertzelMag(samples, n, deg);
		//归一:经验缩放(除以 n*32),对高频段略提振补偿能量下滑
		int64 val = (int64)mag / ((int64)n * 24 + 1);
		//高频段增益补偿(band 越高,乘 (1 + b/nbands))
		val = val * (nbands + b) / nbands;
		if (val > out_max) val = out_max;
		out[b] = (int32)val;
	}
}
