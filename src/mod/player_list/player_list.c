#include "player_list/player_list.h"
#include "modding.h"
#include "recomputils.h"
#include <string.h>

static void safe_string_copy(char *dest, const char *src, int max_len)
{
    int i;
    for (i = 0; i < max_len - 1 && src[i] != '\0'; i++)
    {
        dest[i] = src[i];
    }
    dest[i] = '\0';
}

static PlayerList g_player_list = {0};

void player_list_init(void)
{
    player_list_clear();
}

void player_list_clear(void)
{
    for (int i = 0; i < MAX_PLAYERS_IN_LIST; i++)
    {
        g_player_list.players[i].player_id = 0;
        g_player_list.players[i].username[0] = '\0';
        g_player_list.players[i].is_active = 0;
    }
    g_player_list.player_count = 0;
}

void player_list_add_player(u32 player_id, const char *username)
{
    recomp_printf("[PlayerList] Adding player: ID=%u, Username=%s\n", player_id, username);

    for (int i = 0; i < MAX_PLAYERS_IN_LIST; i++)
    {
        if (g_player_list.players[i].is_active &&
            g_player_list.players[i].player_id == player_id)
        {
            // Update username if changed
            recomp_printf("[PlayerList] Player %u already exists, updating username\n", player_id);
            safe_string_copy(g_player_list.players[i].username, username, sizeof(g_player_list.players[i].username));
            return;
        }
    }

    for (int i = 0; i < MAX_PLAYERS_IN_LIST; i++)
    {
        if (!g_player_list.players[i].is_active)
        {
            g_player_list.players[i].player_id = player_id;
            safe_string_copy(g_player_list.players[i].username, username, sizeof(g_player_list.players[i].username));
            g_player_list.players[i].is_active = 1;
            g_player_list.player_count++;
            recomp_printf("[PlayerList] Player added in slot %d, total count: %d\n", i, g_player_list.player_count);
            return;
        }
    }

    recomp_printf("[PlayerList] WARNING: No empty slot for player %u!\n", player_id);
}

void player_list_remove_player(u32 player_id)
{
    recomp_printf("[PlayerList] Removing player: ID=%u\n", player_id);

    for (int i = 0; i < MAX_PLAYERS_IN_LIST; i++)
    {
        if (g_player_list.players[i].is_active &&
            g_player_list.players[i].player_id == player_id)
        {
            recomp_printf("[PlayerList] Found player %u in slot %d, removing\n", player_id, i);
            g_player_list.players[i].player_id = 0;
            g_player_list.players[i].username[0] = '\0';
            g_player_list.players[i].is_active = 0;
            g_player_list.player_count--;
            recomp_printf("[PlayerList] Player removed, new count: %d\n", g_player_list.player_count);
            return;
        }
    }

    recomp_printf("[PlayerList] WARNING: Player %u not found for removal!\n", player_id);
}

PlayerEntry *player_list_get_player(u32 player_id)
{
    for (int i = 0; i < MAX_PLAYERS_IN_LIST; i++)
    {
        if (g_player_list.players[i].is_active &&
            g_player_list.players[i].player_id == player_id)
        {
            return &g_player_list.players[i];
        }
    }
    return NULL;
}

PlayerEntry *player_list_get_player_at(int index)
{
    int active_count = 0;
    for (int i = 0; i < MAX_PLAYERS_IN_LIST; i++)
    {
        if (g_player_list.players[i].is_active)
        {
            if (active_count == index)
            {
                return &g_player_list.players[i];
            }
            active_count++;
        }
    }
    return NULL;
}

int player_list_get_count(void)
{
    return g_player_list.player_count;
}
