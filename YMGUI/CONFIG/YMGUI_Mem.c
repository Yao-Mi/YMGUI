#include "YMGUI_Mem.h"
#include <stdlib.h>
#include <string.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    YMGUI_Mem.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-01
  *	@Description: 内存双档制,统一内存出入口。移植主战场之一:把大小内存映射到不同物理区
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  *
  * 备注信息：
  * 1.PC/Linux 上大小内存都走 stdlib malloc;裸机上 malloc0→CCM/TCM,malloc1→SDRAM/PSRAM
  * 2.后缀 0/1 是内存区域编号,不是 calloc 语义;调用方必须自行初始化申请到的内存
  *
  ***************************************************************************************************************************
  * <author> <time> <version > <desc>
  * yaomi 26/07/01 1.0 build this moudle
  *
  *   __  __ ___    ____   __  ___ ____     ______ ______ ______ __  __
  *   \ \/ //   |  / __ \ /  |/  //  _/    /_  __// ____// ____// / / /
  *    \  // /| | / / / // /|_/ / / /       / /  / __/  / /    / /_/ /
  *    / // ___ |/ /_/ // /  / /_/ /       / /  / /___ / /___ / __  /
  *   /_//_/  |_|\____//_/  /_//___/      /_/  /_____/ \____//_/ /_/
  *
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

/**
  * @brief 小数据缓冲区申请(高速内存区)
  */
void* GY_malloc0(size_t size)
{
	//不清零:0 表示高速内存区。结构体构造函数须显式初始化全部字段。
	return malloc(size);
}

/**
  * @brief 小数据缓冲区重置长度
  */
void* GY_realloc0(void* ptr, size_t size)
{
	return realloc(ptr, size);
}

/**
  * @brief 小数据缓冲区释放
  */
void GY_free0(void* ptr)
{
	free(ptr);
}

/**
  * @brief 大数据缓冲区申请(低速大内存区)
  */
void* GY_malloc1(size_t size)
{
	return malloc(size);
}

/**
  * @brief 大数据缓冲区重置长度
  */
void* GY_realloc1(void* ptr, size_t size)
{
	return realloc(ptr, size);
}

/**
  * @brief 大数据缓冲区释放
  */
void GY_free1(void* ptr)
{
	free(ptr);
}

/**
  * @brief 内存值设置
  */
void GY_memset(void* ptr, int val, size_t size)
{
	memset(ptr, val, size);
}

/**
  * @brief 内存拷贝
  */
void GY_memcpy(void* dst, const void* src, size_t size)
{
	memcpy(dst, src, size);
}
