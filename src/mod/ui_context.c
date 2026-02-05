#include "ui_context.h"
#include "recompui.h"

static RecompuiContext g_shared_context = RECOMPUI_NULL_CONTEXT;
static int g_context_is_open = 0;
static int g_context_is_shown = 0;

void ui_context_init(void)
{
    if (g_shared_context == RECOMPUI_NULL_CONTEXT)
    {
        g_shared_context = recompui_create_context();
        recompui_set_context_captures_input(g_shared_context, 0);
        recompui_set_context_captures_mouse(g_shared_context, 1);
        g_context_is_open = 0;
        g_context_is_shown = 0;
    }
}

RecompuiContext ui_context_get(void)
{
    return g_shared_context;
}

int ui_context_is_open(void)
{
    return g_context_is_open;
}

void ui_context_open(void)
{
    if (g_shared_context != RECOMPUI_NULL_CONTEXT && !g_context_is_open)
    {
        recompui_open_context(g_shared_context);
        g_context_is_open = 1;
    }
}

void ui_context_close(void)
{
    if (g_shared_context != RECOMPUI_NULL_CONTEXT && g_context_is_open)
    {
        recompui_close_context(g_shared_context);
        g_context_is_open = 0;
    }
}

int ui_context_is_shown(void)
{
    return g_context_is_shown;
}

void ui_context_show(void)
{
    if (g_shared_context != RECOMPUI_NULL_CONTEXT && !g_context_is_shown)
    {
        recompui_show_context(g_shared_context);
        g_context_is_shown = 1;
    }
}

void ui_context_hide(void)
{
    if (g_shared_context != RECOMPUI_NULL_CONTEXT && g_context_is_shown)
    {
        recompui_hide_context(g_shared_context);
        g_context_is_shown = 0;
    }
}
