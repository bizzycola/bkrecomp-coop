#ifndef COOP_NETWORK_H
#define COOP_NETWORK_H

#include "modding.h"
#include "functions.h"

RECOMP_IMPORT(".", int native_lib_test(void));
RECOMP_IMPORT(".", void native_connect_to_server(char *host, char *username, char *lobby_name, char *password));
RECOMP_IMPORT(".", void native_update_network(void));
RECOMP_IMPORT(".", void native_disconnect_from_server(void));
RECOMP_IMPORT(".", void native_sync_jiggy(int jiggy_enum_id, int collected_value));
RECOMP_IMPORT(".", void native_poll_console_input(void));
RECOMP_IMPORT(".", void native_sync_note(int map_id, int level_id, int is_dynamic, int note_index));
RECOMP_IMPORT(".", void native_send_level_opened(int world_id, int jiggy_cost));
RECOMP_IMPORT(".", void native_upload_initial_save_data(void));
RECOMP_IMPORT(".", void native_send_file_progress_flags(void *data, int size));
RECOMP_IMPORT(".", void native_send_honeycomb_collected(int world, int honeycomb_id, int xy_packed, int z));
RECOMP_IMPORT(".", void native_send_mumbo_token_collected(int world, int token_id, int xy_packed, int z));
RECOMP_IMPORT(".", unsigned int GetClockMS(void));

int coop_network_is_safe_now(enum map_e map);

void coop_try_connect_if_ready(enum map_e current_map, int frames_in_map);

void coop_mark_need_initial_upload(void);
void coop_clear_need_initial_upload(void);
int coop_needs_initial_upload(void);

void coop_mark_need_connect(void);

#endif
