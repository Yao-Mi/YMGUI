#include "YMGUI_Trig.h"

/**
  ***************************************************************************************************************************
  *	@FileName:    YMGUI_TrigFunc.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-01
  *	@Description: 定点三角查表函数(取模 + cos 相位偏移)
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

/**
  * @brief 角度归一到 0..359
  */
static int32 normDeg(int32 deg)
{
	deg %= 360;
	if (deg < 0)
		deg += 360;
	return deg;
}

/**
  * @brief sin 定点值(Q15)
  */
int16 GY_Sin(int32 deg)
{
	return GY_SinTab[normDeg(deg)];
}

/**
  * @brief cos 定点值(Q15):cos(x) = sin(x + 90)
  */
int16 GY_Cos(int32 deg)
{
	return GY_SinTab[normDeg(deg + 90)];
}

/**
  * @brief 细角度 sin(Q15):deg64 以 1/64 度为单位,相邻整度线性插值
  */
int32 GY_Sin64(int32 deg64)
{
	int32 whole = deg64 >> 6;        //整数度
	int32 frac = deg64 & 63;         //0..63 小数部分
	int32 a = GY_SinTab[normDeg(whole)];
	int32 b = GY_SinTab[normDeg(whole + 1)];
	return a + (b - a) * frac / 64;  //线性插值
}

/**
  * @brief 细角度 cos(Q15):cos = sin(+90度 = +90*64)
  */
int32 GY_Cos64(int32 deg64)
{
	return GY_Sin64(deg64 + 90 * 64);
}

/**
  * @brief 整数平方根 floor(sqrt(v))。逐位法(binary digit-by-digit),无 FPU、无循环乘除
  *        每次尝试把结果的一个二进制位置1,用加减和移位判定,最坏 16 次迭代
  */
uint16 GY_Isqrt(uint32 v)
{
	uint32 res = 0;
	//bit 从 2^30 起(32位偶数位),逐位下探
	uint32 bit = 1u << 30;
	while (bit > v)
		bit >>= 2;
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
	return (uint16)res;
}
