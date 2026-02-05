#ifndef UI_CONTEXT_H
#define UI_CONTEXT_H

#include "recompui.h"

void ui_context_init(void);
RecompuiContext ui_context_get(void);
int ui_context_is_open(void);
void ui_context_open(void);
void ui_context_close(void);
void ui_context_show(void);
void ui_context_hide(void);
int ui_context_is_shown(void);

#endif
