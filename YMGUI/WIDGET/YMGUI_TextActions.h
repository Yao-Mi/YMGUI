#ifndef YMGUI_TEXT_ACTIONS_H
#define YMGUI_TEXT_ACTIONS_H
#include "YMGUI_Obj.h"
/* Shared touch menu. Update supplies the usable bottom edge (e.g. keyboard top). */
void YMGUI_TextActions_Update(GYCTX ctx, GYcoord bottom, uint8 enabled);
uint8 YMGUI_TextActions_Event(GYOBJ editor, GYEvent event, uint8 multiline);
void YMGUI_TextActions_Detach(GYOBJ editor);
typedef const char* (*GYtext_action_translate_cb)(uint32 key, void* user);
void YMGUI_TextActions_SetTranslator(GYCTX ctx, GYtext_action_translate_cb translate, void* user);
GYOBJ YMGUI_TextActions_GetMenu(GYCTX ctx);
#endif
