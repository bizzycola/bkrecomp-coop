#include "toast.h"
#include "modding.h"
#include "recomputils.h"
#include <string.h>

ToastQueue g_toast_queue = {.head = 0, .tail = 0, .count = 0};
ToastState g_toast_state = {.context = NULL, .display_timer = 0, .is_visible = 0};

static void toast_queue_add(const ToastConfig *config)
{
    if (g_toast_queue.count >= TOAST_QUEUE_SIZE)
    {
        g_toast_queue.tail = (g_toast_queue.tail + 1) % TOAST_QUEUE_SIZE;
        g_toast_queue.count--;
    }

    g_toast_queue.toasts[g_toast_queue.head] = *config;
    g_toast_queue.head = (g_toast_queue.head + 1) % TOAST_QUEUE_SIZE;
    g_toast_queue.count++;
}

static int toast_queue_pop(ToastConfig *out_config)
{
    if (g_toast_queue.count == 0)
    {
        return 0;
    }

    *out_config = g_toast_queue.toasts[g_toast_queue.tail];
    g_toast_queue.tail = (g_toast_queue.tail + 1) % TOAST_QUEUE_SIZE;
    g_toast_queue.count--;
    return 1;
}

static void toast_get_colors(ToastStyle style, RecompuiColor *bg, RecompuiColor *border, RecompuiColor *text)
{
    switch (style)
    {
    case TOAST_STYLE_SUCCESS:
        *bg = (RecompuiColor){.r = 22, .g = 101, .b = 52, .a = 230}; // Green
        *border = (RecompuiColor){.r = 74, .g = 222, .b = 128, .a = 255};
        *text = (RecompuiColor){.r = 240, .g = 253, .b = 244, .a = 255};
        break;
    case TOAST_STYLE_WARNING:
        *bg = (RecompuiColor){.r = 133, .g = 77, .b = 14, .a = 230}; // Orange
        *border = (RecompuiColor){.r = 251, .g = 191, .b = 36, .a = 255};
        *text = (RecompuiColor){.r = 254, .g = 252, .b = 232, .a = 255};
        break;
    case TOAST_STYLE_ERROR:
        *bg = (RecompuiColor){.r = 127, .g = 29, .b = 29, .a = 230}; // Red
        *border = (RecompuiColor){.r = 248, .g = 113, .b = 113, .a = 255};
        *text = (RecompuiColor){.r = 254, .g = 242, .b = 242, .a = 255};
        break;
    case TOAST_STYLE_INFO:
    default:
        *bg = (RecompuiColor){.r = 10, .g = 10, .b = 10, .a = 230}; // Dark gray
        *border = (RecompuiColor){.r = 255, .g = 255, .b = 255, .a = 255};
        *text = (RecompuiColor){.r = 255, .g = 255, .b = 255, .a = 255};
        break;
    }
}

static void toast_get_padding(ToastSize size, float *padding)
{
    switch (size)
    {
    case TOAST_SIZE_SMALL:
        *padding = 8.0f;
        break;
    case TOAST_SIZE_LARGE:
        *padding = 16.0f;
        break;
    case TOAST_SIZE_MEDIUM:
    default:
        *padding = 12.0f;
        break;
    }
}

static void toast_apply_position(RecompuiResource root, ToastPosition position)
{
    recompui_set_position(root, POSITION_ABSOLUTE);

    float margin = 32.0f;

    switch (position)
    {
    case TOAST_POS_TOP_LEFT:
        recompui_set_top(root, margin, UNIT_DP);
        recompui_set_left(root, margin, UNIT_DP);
        break;
    case TOAST_POS_TOP_CENTER:
        recompui_set_top(root, margin, UNIT_DP);
        recompui_set_left(root, 0.0f, UNIT_DP);
        recompui_set_right(root, 0.0f, UNIT_DP);
        recompui_set_margin_left_auto(root);
        recompui_set_margin_right_auto(root);
        break;
    case TOAST_POS_TOP_RIGHT:
        recompui_set_top(root, margin, UNIT_DP);
        recompui_set_right(root, margin, UNIT_DP);
        break;
    case TOAST_POS_BOTTOM_LEFT:
        recompui_set_bottom(root, margin, UNIT_DP);
        recompui_set_left(root, margin, UNIT_DP);
        break;
    case TOAST_POS_BOTTOM_CENTER:
        recompui_set_bottom(root, margin, UNIT_DP);
        recompui_set_left(root, 0.0f, UNIT_DP);
        recompui_set_right(root, 0.0f, UNIT_DP);
        recompui_set_margin_left_auto(root);
        recompui_set_margin_right_auto(root);
        break;
    case TOAST_POS_BOTTOM_RIGHT:
        recompui_set_bottom(root, margin, UNIT_DP);
        recompui_set_right(root, margin, UNIT_DP);
        break;
    case TOAST_POS_CENTER:
        recompui_set_top(root, 0.0f, UNIT_DP);
        recompui_set_bottom(root, 0.0f, UNIT_DP);
        recompui_set_left(root, 0.0f, UNIT_DP);
        recompui_set_right(root, 0.0f, UNIT_DP);
        recompui_set_margin_auto(root);
        break;
    }

    recompui_set_width_auto(root);
    recompui_set_height_auto(root);
}

static void toast_display_internal(const ToastConfig *config)
{
    if (g_toast_state.context != NULL)
    {
        recompui_hide_context(g_toast_state.context);
    }

    g_toast_state.context = recompui_create_context();
    recompui_set_context_captures_input(g_toast_state.context, 0);
    recompui_set_context_captures_mouse(g_toast_state.context, 0);
    recompui_open_context(g_toast_state.context);

    RecompuiResource root = recompui_context_root(g_toast_state.context);
    toast_apply_position(root, config->position);

    RecompuiColor bg_color, border_color, text_color;
    toast_get_colors(config->style, &bg_color, &border_color, &text_color);

    float padding;
    toast_get_padding(config->size, &padding);

    RecompuiResource container = recompui_create_element(g_toast_state.context, root);
    recompui_set_background_color(container, &bg_color);
    recompui_set_padding(container, padding, UNIT_DP);
    recompui_set_border_radius(container, 8.0f, UNIT_DP);
    recompui_set_border_width(container, 2.0f, UNIT_DP);
    recompui_set_border_color(container, &border_color);

    recompui_create_label(g_toast_state.context, container, config->message, LABELSTYLE_NORMAL);

    recompui_close_context(g_toast_state.context);
    recompui_show_context(g_toast_state.context);

    g_toast_state.is_visible = 1;
    g_toast_state.display_timer = config->duration_frames;
    g_toast_state.current = *config;
}

void toast_init(void)
{
    g_toast_queue.head = 0;
    g_toast_queue.tail = 0;
    g_toast_queue.count = 0;
    g_toast_state.context = NULL;
    g_toast_state.display_timer = 0;
    g_toast_state.is_visible = 0;
}

void toast_update(void)
{
    if (g_toast_state.is_visible)
    {
        g_toast_state.display_timer--;
        if (g_toast_state.display_timer <= 0)
        {
            toast_hide_current();

            ToastConfig next;
            if (toast_queue_pop(&next))
            {
                toast_display_internal(&next);
            }
        }
    }
    else
    {
        ToastConfig next;
        if (toast_queue_pop(&next))
        {
            toast_display_internal(&next);
        }
    }
}

void toast_show_custom(const char *message, int duration_frames,
                       ToastPosition position, ToastSize size, ToastStyle style)
{
    if (!message)
        return;

    ToastConfig config = {0};

    int i = 0;
    while (message[i] != '\0' && i < TOAST_MAX_LENGTH - 1)
    {
        config.message[i] = message[i];
        i++;
    }
    config.message[i] = '\0';

    config.duration_frames = (duration_frames > 0) ? duration_frames : TOAST_DEFAULT_DURATION;
    config.position = position;
    config.size = size;
    config.style = style;

    if (!g_toast_state.is_visible)
    {
        toast_display_internal(&config);
    }
    else
    {
        // Queue it
        toast_queue_add(&config);
    }
}

void toast_show(const char *message)
{
    toast_show_custom(message, TOAST_DEFAULT_DURATION,
                      TOAST_POS_TOP_RIGHT, TOAST_SIZE_MEDIUM, TOAST_STYLE_INFO);
}

void toast_show_timed(const char *message, int duration_frames)
{
    toast_show_custom(message, duration_frames,
                      TOAST_POS_TOP_RIGHT, TOAST_SIZE_MEDIUM, TOAST_STYLE_INFO);
}

void toast_show_immediate_custom(const char *message, int duration_frames,
                                 ToastPosition position, ToastSize size, ToastStyle style)
{
    if (!message)
        return;

    ToastConfig config = {0};

    int i = 0;
    while (message[i] != '\0' && i < TOAST_MAX_LENGTH - 1)
    {
        config.message[i] = message[i];
        i++;
    }
    config.message[i] = '\0';

    config.duration_frames = (duration_frames > 0) ? duration_frames : TOAST_DEFAULT_DURATION;
    config.position = position;
    config.size = size;
    config.style = style;

    toast_clear_queue();
    toast_hide_current();

    toast_display_internal(&config);
}

void toast_show_immediate(const char *message)
{
    toast_show_immediate_custom(message, TOAST_DEFAULT_DURATION,
                                TOAST_POS_TOP_RIGHT, TOAST_SIZE_MEDIUM, TOAST_STYLE_INFO);
}

void toast_clear_queue(void)
{
    g_toast_queue.head = 0;
    g_toast_queue.tail = 0;
    g_toast_queue.count = 0;
}

void toast_hide_current(void)
{
    if (g_toast_state.context != NULL)
    {
        recompui_hide_context(g_toast_state.context);
        g_toast_state.context = NULL;
    }
    g_toast_state.is_visible = 0;
    g_toast_state.display_timer = 0;
}

void toast_info(const char *message)
{
    toast_show_custom(message, TOAST_DEFAULT_DURATION,
                      TOAST_POS_TOP_RIGHT, TOAST_SIZE_MEDIUM, TOAST_STYLE_INFO);
}

void toast_success(const char *message)
{
    toast_show_custom(message, TOAST_DEFAULT_DURATION,
                      TOAST_POS_TOP_RIGHT, TOAST_SIZE_MEDIUM, TOAST_STYLE_SUCCESS);
}

void toast_warning(const char *message)
{
    toast_show_custom(message, TOAST_DEFAULT_DURATION,
                      TOAST_POS_TOP_RIGHT, TOAST_SIZE_MEDIUM, TOAST_STYLE_WARNING);
}

void toast_error(const char *message)
{
    toast_show_custom(message, TOAST_DEFAULT_DURATION,
                      TOAST_POS_TOP_RIGHT, TOAST_SIZE_MEDIUM, TOAST_STYLE_ERROR);
}
