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
#if defined(GY_TOUCH_MAX) && GY_TOUCH_MAX != 5
#error "YMGUI SDK uses GY_TOUCH_MAX=5; rebuild to customize"
#endif
#ifndef GY_TOUCH_MAX
#define GY_TOUCH_MAX 5
#endif
#if defined(YMGUI_OCCLUSION) && YMGUI_OCCLUSION != 0
#error "YMGUI SDK uses YMGUI_OCCLUSION=0; rebuild to customize"
#endif
#ifndef YMGUI_OCCLUSION
#define YMGUI_OCCLUSION 0
#endif
#if defined(YMGUI_CLIPBOARD_DYNAMIC) && YMGUI_CLIPBOARD_DYNAMIC != 0
#error "YMGUI SDK uses YMGUI_CLIPBOARD_DYNAMIC=0; rebuild to customize"
#endif
#ifndef YMGUI_CLIPBOARD_DYNAMIC
#define YMGUI_CLIPBOARD_DYNAMIC 0
#endif
#if defined(YMGUI_RESOURCE_STDIO) && YMGUI_RESOURCE_STDIO != 1
#error "YMGUI SDK uses YMGUI_RESOURCE_STDIO=1; rebuild to customize"
#endif
#ifndef YMGUI_RESOURCE_STDIO
#define YMGUI_RESOURCE_STDIO 1
#endif
#endif
