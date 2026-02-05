#ifndef COOP_PLAYER_HANDLERS_H
#define COOP_PLAYER_HANDLERS_H

void handle_player_connected(const void *msg);
void handle_player_disconnected(const void *msg);
void handle_player_list_update(const void *msg);
void handle_player_info_request(const void *msg);
void handle_player_info_response(const void *msg);

#endif
