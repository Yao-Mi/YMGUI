#ifndef MP_SPECTRUM_H
#define MP_SPECTRUM_H

#include "YMGUI_PubType.h"

//===========================================================================
// mp_spectrum —— 频段能量分析(app 侧,库不做音视频分析)。
//   对一窗 mono PCM(int16)做定长整数 DFT(Goertzel 按对数分布的中心频率),
//   取幅度归一到 0..out_max,填进 nbands 个柱。无 libm(用库的 Q15 三角表)。
//   目标:能动、跟音乐大致相关,不追求真 FFT 精度(定调:库核心是 GUI)。
//===========================================================================

#define MP_SPEC_MAX_BANDS 32

//分析一窗 PCM:
//  samples  : mono int16 样本(若源是立体声,调用方先混成 mono)
//  n        : 样本数(建议 512~1024)
//  rate     : 采样率(Hz)
//  nbands   : 需要的频段数(<=MP_SPEC_MAX_BANDS)
//  out      : 输出 nbands 个能量值(0..out_max)
//  out_max  : 输出满刻度
void mp_spectrum_analyze(const int16* samples, int32 n, int32 rate,
                         int32 nbands, int32* out, int32 out_max);

#endif // !MP_SPECTRUM_H
