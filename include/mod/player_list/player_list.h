#ifndef PLAYER_LIST_H
#define PLAYER_LIST_H

#include <ultra64.h>

#define MAX_PLAYERS_IN_LIST 16
#define MAX_USERNAME_LENGTH 32

typedef struct
{
    u32 player_id;
    char username[32];
    int is_active;
} PlayerEntry;

typedef struct
{
    PlayerEntry players[MAX_PLAYERS_IN_LIST];
    int player_count;
} PlayerList;

void player_list_init(void);
void player_list_clear(void);
void player_list_add_player(u32 player_id, const char *username);
void player_list_remove_player(u32 player_id);
PlayerEntry *player_list_get_player(u32 player_id);
PlayerEntry *player_list_get_player_at(int index);
int player_list_get_count(void);

#endif
