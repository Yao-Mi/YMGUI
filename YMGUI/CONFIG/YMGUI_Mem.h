#ifndef YMGUI_MEM_H
#define YMGUI_MEM_H

#include "YMGUI_PubType.h"

//内存双档制(对标 YMCV CV_malloc0/1),移植时只改本文件 .c 的函数体,签名不动
//  malloc0 = 小/快(高速内存 CCM/TCM):对象头、样式、事件、脏矩形表、字形表、渐变LUT
//  malloc1 = 大/慢(低速大内存 SDRAM/PSRAM):draw buffer、framebuffer、解码图、字形位图
void* GY_malloc0(size_t size);            //小数据缓冲区申请(高速区)
void* GY_realloc0(void* ptr, size_t size);//重置长度
void  GY_free0(void* ptr);                //释放

void* GY_malloc1(size_t size);            //大数据缓冲区申请(低速大内存区)
void* GY_realloc1(void* ptr, size_t size);//重置长度
void  GY_free1(void* ptr);                //释放

void  GY_memset(void* ptr, int val, size_t size);       //内存值设置
void  GY_memcpy(void* dst, const void* src, size_t size);//内存拷贝

#endif // !YMGUI_MEM_H
