#include "player_list/player_list_ui.h"
#include "player_list/player_list.h"
#include "ui_context.h"
#include "recomputils.h"
#include "modding.h"
#include "console/console.h"
#include <string.h>

RECOMP_IMPORT(".", void native_send_player_info_request(u32 target_player_id, u32 requester_player_id));

static RecompuiResource g_root = 0;
static RecompuiResource g_container = 0;
static RecompuiResource g_title_button = 0;
static RecompuiResource g_player_buttons[MAX_PLAYERS_IN_LIST];
static int g_player_button_count = 0;
static int g_ui_visible = 0;
static int g_list_expanded = 1;
static int g_needs_rebuild = 0;
static u32 g_clicked_player_ids[MAX_PLAYERS_IN_LIST];
static int g_first_build = 1;
static RecompuiResource g_context_root = NULL;

static void on_toggle_list_clicked(RecompuiResource resource, const RecompuiEventData *event, void *userdata)
{
    (void)resource;
    (void)userdata;

    if (event->type == UI_EVENT_CLICK)
    {
        g_list_expanded = !g_list_expanded;
        g_needs_rebuild = 1;
    }
}

static void on_player_name_clicked(RecompuiResource resource, const RecompuiEventData *event, void *userdata)
{
    if (event->type == UI_EVENT_CLICK)
    {
        u32 player_id = *(u32 *)userdata;

        native_send_player_info_request(player_id, 0);
    }
}

static int strings_equal_case_insensitive(const char *a, const char *b)
{
    while (*a && *b)
    {
        char ca = *a;
        char cb = *b;

        // Convert to lowercase
        if (ca >= 'A' && ca <= 'Z')
            ca += 32;
        if (cb >= 'A' && cb <= 'Z')
            cb += 32;

        if (ca != cb)
            return 0;

        a++;
        b++;
    }
    return *a == *b;
}

static int cmd_teleport(int argc, char **argv)
{
    if (argc < 2)
    {
        console_log_error("Usage: teleport <player name>");
        return 0;
    }

    char player_name[MAX_USERNAME_LENGTH * 2] = {0};
    int pos = 0;

    for (int i = 1; i < argc && pos < sizeof(player_name) - 1; i++)
    {
        if (i > 1)
        {
            player_name[pos++] = ' ';
        }

        char *arg = argv[i];
        while (*arg && pos < sizeof(player_name) - 1)
        {
            player_name[pos++] = *arg++;
        }
    }
    player_name[pos] = '\0';

    int player_count = player_list_get_count();
    u32 target_player_id = 0;
    int found = 0;

    for (int i = 0; i < player_count; i++)
    {
        PlayerEntry *player = player_list_get_player_at(i);
        if (player != NULL && player->is_active)
        {
            if (strings_equal_case_insensitive(player->username, player_name))
            {
                target_player_id = player->player_id;
                found = 1;
                break;
            }
        }
    }

    if (!found)
    {
        console_log_error("Player not found");
        return 0;
    }

    native_send_player_info_request(target_player_id, 0);
    console_log_success("Teleporting to player...");

    return 1;
}

void player_list_ui_init(void)
{
    g_first_build = 1;
    g_ui_visible = 0;

    console_register_command("teleport", cmd_teleport, "Teleport to a player by name");
}

void player_list_ui_show(void)
{
    if (g_ui_visible)
    {
        return;
    }

    g_ui_visible = 1;
    player_list_ui_rebuild();
}

void player_list_ui_hide(void)
{
    if (!g_ui_visible)
    {
        return;
    }

    g_ui_visible = 0;

    if (g_root != 0)
    {
        if (!ui_context_is_open())
        {
            ui_context_open();
        }
        recompui_set_display(g_root, DISPLAY_NONE);
        ui_context_close();
    }
}

void player_list_ui_toggle(void)
{
    if (g_ui_visible)
    {
        player_list_ui_hide();
    }
    else
    {
        player_list_ui_show();
    }
}

void player_list_ui_update(void)
{
    if (g_needs_rebuild)
    {
        g_needs_rebuild = 0;
        player_list_ui_rebuild();
    }
}

void player_list_create_root(void)
{
    RecompuiContext ctx = ui_context_get();
    RecompuiResource context_root = recompui_context_root(ctx);

    g_root = recompui_create_element(ctx, context_root);
    recompui_set_position(g_root, POSITION_ABSOLUTE);
    recompui_set_top(g_root, 20.0f, UNIT_DP);
    recompui_set_left(g_root, 20.0f, UNIT_DP);
    recompui_set_width_auto(g_root);
    recompui_set_height_auto(g_root);

    g_container = recompui_create_element(ctx, g_root);
    recompui_set_display(g_container, DISPLAY_FLEX);
    recompui_set_flex_direction(g_container, FLEX_DIRECTION_COLUMN);
    recompui_set_gap(g_container, 5.0f, UNIT_DP);
    recompui_set_padding(g_container, 10.0f, UNIT_DP);
    recompui_set_border_radius(g_container, 4.0f, UNIT_DP);
    recompui_set_border_width(g_container, 1.0f, UNIT_DP);

    RecompuiColor bg_color = {.r = 10, .g = 10, .b = 10, .a = 200};
    RecompuiColor border_color = {.r = 100, .g = 100, .b = 100, .a = 255};
    recompui_set_background_color(g_container, &bg_color);
    recompui_set_border_color(g_container, &border_color);

    const char *title_text = g_list_expanded ? "Players \xE2\x96\xBC" : "Players \xE2\x96\xB6";
    g_title_button = recompui_create_button(ctx, g_container, title_text, BUTTONSTYLE_SECONDARY);
    recompui_register_callback(g_title_button, on_toggle_list_clicked, NULL);
}

void create_player_buttons(int player_count)
{
    RecompuiContext ctx = ui_context_get();

    if (player_count == 0)
    {
        g_player_buttons[0] = recompui_create_label(ctx, g_container, "No players", LABELSTYLE_SMALL);
        RecompuiColor gray_color = {.r = 150, .g = 150, .b = 150, .a = 255};
        recompui_set_color(g_player_buttons[0], &gray_color);
        g_player_button_count = 1;
    }
    else
    {
        for (int i = 0; i < player_count && i < MAX_PLAYERS_IN_LIST; i++)
        {
            PlayerEntry *player = player_list_get_player_at(i);
            if (player != NULL && player->is_active)
            {
                g_player_buttons[i] = recompui_create_button(ctx, g_container, player->username, BUTTONSTYLE_PRIMARY);
                g_clicked_player_ids[i] = player->player_id;
                recompui_register_callback(g_player_buttons[i], on_player_name_clicked, &g_clicked_player_ids[i]);
                g_player_button_count++;
            }
        }
    }
}

void delete_player_buttons()
{
    for (int i = 0; i < g_player_button_count; i++)
    {
        if (g_player_buttons[i] != 0)
        {
            recompui_set_display(g_player_buttons[i], DISPLAY_NONE);
            recompui_set_width(g_player_buttons[i], 0.0f, UNIT_DP);
            recompui_set_height(g_player_buttons[i], 0.0f, UNIT_DP);
        }
    }
}

void player_list_ui_rebuild(void)
{
    int player_count = player_list_get_count();

    RecompuiContext ctx = ui_context_get();

    if (!ui_context_is_open())
    {
        ui_context_open();
    }

    if (g_first_build)
    {
        player_list_create_root();
        g_first_build = 0;
    }
    else
    {
        if (g_title_button != 0)
        {
            const char *title_text = g_list_expanded ? "Players \xE2\x96\xBC" : "Players \xE2\x96\xB6";
            recompui_set_text(g_title_button, title_text);
        }

        // Hide old buttons
        delete_player_buttons();
        g_player_button_count = 0;
    }

    if (g_root != 0)
    {
        recompui_set_display(g_root, g_ui_visible ? DISPLAY_FLEX : DISPLAY_NONE);
    }

    if (g_list_expanded)
    {
        create_player_buttons(player_count);
    }

    ui_context_close();

    if (g_ui_visible)
    {
        ui_context_show();
    }
}
