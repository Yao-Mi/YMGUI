#ifndef YMGUI_SDK_CONFIG_H
#define YMGUI_SDK_CONFIG_H
/* Precompiled SDK ABI. Rebuild the library to customize these settings. */
#ifndef YMGUI_SDK_COLOR_DEPTH
#define YMGUI_SDK_COLOR_DEPTH 16
#endif
#if YMGUI_SDK_COLOR_DEPTH != 16 && YMGUI_SDK_COLOR_DEPTH != 24
#error "YMGUI SDK supports only RGB565 and RGB888"
#endif
#ifndef YMGUI_COLOR_DEPTH
#define YMGUI_COLOR_DEPTH YMGUI_SDK_COLOR_DEPTH
#endif
#if YMGUI_COLOR_DEPTH != YMGUI_SDK_COLOR_DEPTH
#error "YMGUI SDK color depth does not match the imported library"
#endif
#ifdef YMGUI_COORD_32
#error "YMGUI SDK uses 16-bit coordinates; rebuild for YMGUI_COORD_32"
#endif
#if defined(GY_INV_MAX) && GY_INV_MAX != 16
#error "YMGUI SDK uses GY_INV_MAX=16"
#endif
#ifndef YMGUI_SDK_ANIM
#define YMGUI_SDK_ANIM 1
#endif
#ifndef YMGUI_ANIM
#define YMGUI_ANIM YMGUI_SDK_ANIM
#endif
#if YMGUI_ANIM != YMGUI_SDK_ANIM
#error "YMGUI SDK animation configuration does not match the imported library"
#endif
#endif
