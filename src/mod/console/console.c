#include "console/console.h"
#include "ui_context.h"
#include "recomputils.h"
#include "modding.h"
#include <string.h>

static struct
{
    int is_visible;
    int needs_rebuild;
    char input_buffer[CONSOLE_MAX_INPUT];
    int input_length;
    char history[CONSOLE_MAX_HISTORY][CONSOLE_MAX_INPUT];
    int history_count;
    int history_start;
    RecompuiResource root;
    RecompuiResource container;
    RecompuiResource history_labels[CONSOLE_MAX_HISTORY];
    RecompuiResource input_label;
} g_console = {0};

static ConsoleCommand g_commands[CONSOLE_MAX_COMMANDS] = {0};
static int g_command_count = 0;

static void safe_strcpy(char *dest, const char *src, int max_len)
{
    int i;
    for (i = 0; i < max_len - 1 && src[i] != '\0'; i++)
    {
        dest[i] = src[i];
    }
    dest[i] = '\0';
}

static void safe_strcat(char *dest, const char *src, int max_len)
{
    int dest_len = 0;
    while (dest[dest_len] != '\0' && dest_len < max_len - 1)
    {
        dest_len++;
    }

    int i = 0;
    while (src[i] != '\0' && dest_len < max_len - 1)
    {
        dest[dest_len++] = src[i++];
    }
    dest[dest_len] = '\0';
}

static int str_equals(const char *a, const char *b)
{
    int i = 0;
    while (a[i] != '\0' && b[i] != '\0')
    {
        if (a[i] != b[i])
            return 0;
        i++;
    }
    return a[i] == b[i];
}

static void add_to_history(const char *line)
{
    if (g_console.history_count < CONSOLE_MAX_HISTORY)
    {
        safe_strcpy(g_console.history[g_console.history_count], line, CONSOLE_MAX_INPUT);
        g_console.history_count++;
    }
    else
    {
        safe_strcpy(g_console.history[g_console.history_start], line, CONSOLE_MAX_INPUT);
        g_console.history_start = (g_console.history_start + 1) % CONSOLE_MAX_HISTORY;
    }
    g_console.needs_rebuild = 1;
}

static int parse_command(const char *input, int *argc, char argv[][CONSOLE_MAX_INPUT])
{
    *argc = 0;
    int in_arg = 0;
    int arg_start = 0;
    int arg_len = 0;

    for (int i = 0; input[i] != '\0' && *argc < 16; i++)
    {
        if (input[i] == ' ' || input[i] == '\t')
        {
            if (in_arg)
            {
                argv[*argc][arg_len] = '\0';
                (*argc)++;
                in_arg = 0;
                arg_len = 0;
            }
        }
        else
        {
            if (!in_arg)
            {
                in_arg = 1;
                arg_len = 0;
            }

            if (arg_len < CONSOLE_MAX_INPUT - 1)
            {
                argv[*argc][arg_len++] = input[i];
            }
        }
    }

    if (in_arg)
    {
        argv[*argc][arg_len] = '\0';
        (*argc)++;
    }

    return *argc > 0;
}

static void execute_command(const char *input)
{
    char cmd_line[CONSOLE_MAX_INPUT + 2];
    cmd_line[0] = '>';
    cmd_line[1] = ' ';
    safe_strcpy(cmd_line + 2, input, CONSOLE_MAX_INPUT - 2);
    add_to_history(cmd_line);

    // Parse command
    int argc;
    char argv[16][CONSOLE_MAX_INPUT];
    char *argv_ptrs[16];

    if (!parse_command(input, &argc, argv))
    {
        console_log_error("Empty command");
        return;
    }

    for (int i = 0; i < argc; i++)
    {
        argv_ptrs[i] = argv[i];
    }

    for (int i = 0; i < g_command_count; i++)
    {
        if (g_commands[i].is_active && str_equals(g_commands[i].name, argv[0]))
        {
            int result = g_commands[i].handler(argc, argv_ptrs);
            if (!result)
            {
                console_log_error("Command failed");
            }
            return;
        }
    }

    console_log_error("Unknown command. Type 'help' for commands.");
}

static int cmd_help(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    console_log_info("Available commands:");
    for (int i = 0; i < g_command_count; i++)
    {
        if (g_commands[i].is_active)
        {
            char line[256];
            safe_strcpy(line, "  ", 256);
            safe_strcat(line, g_commands[i].name, 256);
            if (g_commands[i].description)
            {
                safe_strcat(line, " - ", 256);
                safe_strcat(line, g_commands[i].description, 256);
            }
            console_log(line);
        }
    }
    return 1;
}

static int cmd_clear(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    g_console.history_count = 0;
    g_console.history_start = 0;
    g_console.needs_rebuild = 1;
    return 1;
}

static int cmd_echo(int argc, char **argv)
{
    if (argc < 2)
    {
        console_log_error("Usage: echo <message>");
        return 0;
    }

    char message[CONSOLE_MAX_INPUT];
    message[0] = '\0';

    for (int i = 1; i < argc; i++)
    {
        if (i > 1)
            safe_strcat(message, " ", CONSOLE_MAX_INPUT);
        safe_strcat(message, argv[i], CONSOLE_MAX_INPUT);
    }

    console_log(message);
    return 1;
}

static void console_create_ui(void)
{
    RecompuiContext ctx = ui_context_get();
    RecompuiResource context_root = recompui_context_root(ctx);

    g_console.root = recompui_create_element(ctx, context_root);
    recompui_set_position(g_console.root, POSITION_ABSOLUTE);
    recompui_set_bottom(g_console.root, 0.0f, UNIT_DP);
    recompui_set_left(g_console.root, 0.0f, UNIT_DP);
    recompui_set_right(g_console.root, 0.0f, UNIT_DP);
    recompui_set_height(g_console.root, 350.0f, UNIT_DP);

    g_console.container = recompui_create_element(ctx, g_console.root);
    recompui_set_display(g_console.container, DISPLAY_FLEX);
    recompui_set_flex_direction(g_console.container, FLEX_DIRECTION_COLUMN);
    recompui_set_width(g_console.container, 100.0f, UNIT_PERCENT);
    recompui_set_height(g_console.container, 100.0f, UNIT_PERCENT);
    recompui_set_padding(g_console.container, 10.0f, UNIT_DP);
    recompui_set_gap(g_console.container, 2.0f, UNIT_DP);

    RecompuiColor bg_color = {.r = 0, .g = 0, .b = 0, .a = 220};
    RecompuiColor border_color = {.r = 100, .g = 100, .b = 100, .a = 255};
    recompui_set_background_color(g_console.container, &bg_color);
    recompui_set_border_width(g_console.container, 2.0f, UNIT_DP);
    recompui_set_border_color(g_console.container, &border_color);

    RecompuiResource title = recompui_create_label(ctx, g_console.container, "=== Console ===", LABELSTYLE_NORMAL);
    RecompuiColor title_color = {.r = 150, .g = 200, .b = 255, .a = 255};
    recompui_set_color(title, &title_color);

    for (int i = 0; i < CONSOLE_MAX_HISTORY; i++)
    {
        g_console.history_labels[i] = recompui_create_label(ctx, g_console.container, "", LABELSTYLE_SMALL);
        RecompuiColor text_color = {.r = 200, .g = 200, .b = 200, .a = 255};
        recompui_set_color(g_console.history_labels[i], &text_color);
    }

    char input_display[CONSOLE_MAX_INPUT + 3];
    input_display[0] = '>';
    input_display[1] = ' ';
    safe_strcpy(input_display + 2, g_console.input_buffer, CONSOLE_MAX_INPUT);
    safe_strcat(input_display, "_", CONSOLE_MAX_INPUT + 3); // Cursor

    g_console.input_label = recompui_create_label(ctx, g_console.container, input_display, LABELSTYLE_NORMAL);
    RecompuiColor input_color = {.r = 255, .g = 255, .b = 255, .a = 255};
    recompui_set_color(g_console.input_label, &input_color);
}

static void console_update_ui(void)
{
    if (!g_console.needs_rebuild)
        return;

    if (!ui_context_is_open())
    {
        ui_context_open();
    }

    for (int i = 0; i < CONSOLE_MAX_HISTORY; i++)
    {
        if (i < g_console.history_count)
        {
            int idx = (g_console.history_start + i) % CONSOLE_MAX_HISTORY;
            recompui_set_text(g_console.history_labels[i], g_console.history[idx]);
            recompui_set_display(g_console.history_labels[i], DISPLAY_BLOCK);
        }
        else
        {
            recompui_set_text(g_console.history_labels[i], "");
            recompui_set_display(g_console.history_labels[i], DISPLAY_NONE);
        }
    }

    char input_display[CONSOLE_MAX_INPUT + 3];
    input_display[0] = '>';
    input_display[1] = ' ';
    safe_strcpy(input_display + 2, g_console.input_buffer, CONSOLE_MAX_INPUT);
    safe_strcat(input_display, "_", CONSOLE_MAX_INPUT + 3);
    recompui_set_text(g_console.input_label, input_display);

    ui_context_close();
    g_console.needs_rebuild = 0;
}

void console_init(void)
{
    g_console.is_visible = 0;
    g_console.needs_rebuild = 0;
    g_console.input_length = 0;
    g_console.input_buffer[0] = '\0';
    g_console.history_count = 0;
    g_console.history_start = 0;

    console_register_command("help", cmd_help, "Show available commands");
    console_register_command("clear", cmd_clear, "Clear console history");
    console_register_command("echo", cmd_echo, "Echo message");

    if (!ui_context_is_open())
    {
        ui_context_open();
    }
    console_create_ui();
    recompui_set_display(g_console.root, DISPLAY_NONE);
    ui_context_close();
    ui_context_show();

    console_log_info("Console initialized. Press ~ to toggle.");
}

void console_show(void)
{
    if (g_console.is_visible)
        return;

    g_console.is_visible = 1;

    if (g_console.root == 0)
    {
        if (!ui_context_is_open())
        {
            ui_context_open();
        }
        console_create_ui();
        ui_context_close();
    }

    if (!ui_context_is_open())
    {
        ui_context_open();
    }
    recompui_set_display(g_console.root, DISPLAY_FLEX);
    ui_context_close();

    RecompuiContext ctx = ui_context_get();
    recompui_set_context_captures_input(ctx, 1);
}

void console_hide(void)
{
    if (!g_console.is_visible)
        return;

    g_console.is_visible = 0;

    RecompuiContext ctx = ui_context_get();
    recompui_set_context_captures_input(ctx, 0);

    if (g_console.root != 0)
    {
        if (!ui_context_is_open())
        {
            ui_context_open();
        }
        recompui_set_display(g_console.root, DISPLAY_NONE);
        ui_context_close();
    }
}

void console_toggle(void)
{
    if (g_console.is_visible)
    {
        console_hide();
    }
    else
    {
        console_show();
    }
}

int console_is_visible(void)
{
    return g_console.is_visible;
}

void console_log(const char *message)
{
    add_to_history(message);
}

void console_log_info(const char *message)
{
    char line[CONSOLE_MAX_INPUT];
    safe_strcpy(line, "[INFO] ", CONSOLE_MAX_INPUT);
    safe_strcat(line, message, CONSOLE_MAX_INPUT);
    add_to_history(line);
}

void console_log_success(const char *message)
{
    char line[CONSOLE_MAX_INPUT];
    safe_strcpy(line, "[OK] ", CONSOLE_MAX_INPUT);
    safe_strcat(line, message, CONSOLE_MAX_INPUT);
    add_to_history(line);
}

void console_log_error(const char *message)
{
    char line[CONSOLE_MAX_INPUT];
    safe_strcpy(line, "[ERROR] ", CONSOLE_MAX_INPUT);
    safe_strcat(line, message, CONSOLE_MAX_INPUT);
    add_to_history(line);
}

void console_log_warning(const char *message)
{
    char line[CONSOLE_MAX_INPUT];
    safe_strcpy(line, "[WARN] ", CONSOLE_MAX_INPUT);
    safe_strcat(line, message, CONSOLE_MAX_INPUT);
    add_to_history(line);
}

int console_register_command(const char *name, ConsoleCommandHandler handler, const char *description)
{
    if (g_command_count >= CONSOLE_MAX_COMMANDS)
        return 0;

    safe_strcpy(g_commands[g_command_count].name, name, CONSOLE_MAX_COMMAND_NAME);
    g_commands[g_command_count].handler = handler;
    g_commands[g_command_count].description = description;
    g_commands[g_command_count].is_active = 1;
    g_command_count++;

    return 1;
}

void console_handle_key(char key)
{
    if (!g_console.is_visible)
        return;

    if (g_console.input_length < CONSOLE_MAX_INPUT - 1)
    {
        g_console.input_buffer[g_console.input_length++] = key;
        g_console.input_buffer[g_console.input_length] = '\0';
        g_console.needs_rebuild = 1;
    }
}

void console_handle_backspace(void)
{
    if (!g_console.is_visible)
        return;

    if (g_console.input_length > 0)
    {
        g_console.input_length--;
        g_console.input_buffer[g_console.input_length] = '\0';
        g_console.needs_rebuild = 1;
    }
}

void console_handle_enter(void)
{
    if (!g_console.is_visible)
        return;

    if (g_console.input_length > 0)
    {
        execute_command(g_console.input_buffer);
        g_console.input_buffer[0] = '\0';
        g_console.input_length = 0;
        g_console.needs_rebuild = 1;
    }
}

void console_update(void)
{
    if (g_console.is_visible && g_console.needs_rebuild)
    {
        console_update_ui();
    }
}
