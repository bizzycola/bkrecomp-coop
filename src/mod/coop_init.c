// =========================================================================== //
// This is the recomp mod file.
//
// It handles all game code, and calls our lib_main functions as needed
// to handle networking.
// =========================================================================== //

#include "modding.h"

#include "functions.h"
#include "variables.h"
#include "recomputils.h"
#include "recompconfig.h"
#include "recompui.h"
#include "recompdata.h"
#include "puppets/puppet.h"
#include "message_queue/message_queue.h"
#include "toast/toast.h"
#include "sync/sync.h"
#include "collection/collection.h"
#include "bkrecomp_api.h"
#include "player_list/player_list.h"
#include "player_list/player_list_ui.h"
#include "ui_context.h"
#include "console/console.h"

RECOMP_IMPORT(".", int native_lib_test(void));
RECOMP_IMPORT(".", void native_connect_to_server(char *host, char *username, char *lobby_name, char *password));
RECOMP_IMPORT(".", void native_update_network(void));
RECOMP_IMPORT(".", void native_disconnect_from_server(void));
RECOMP_IMPORT(".", void native_sync_jiggy(int jiggy_enum_id, int collected_value));
RECOMP_IMPORT(".", void native_poll_console_input(void));
RECOMP_IMPORT(".", void native_sync_note(int map_id, int level_id, int is_dynamic, int note_index));
RECOMP_IMPORT(".", void native_send_level_opened(int world_id, int jiggy_cost));
RECOMP_IMPORT(".", void native_upload_initial_save_data(void));
RECOMP_IMPORT(".", void native_send_file_progress_flags(u8 *data, int size));
RECOMP_IMPORT(".", void native_send_ability_progress(void *moves_data, int size));
RECOMP_IMPORT(".", void native_send_honeycomb_score(void *score_data, int size));
RECOMP_IMPORT(".", void native_send_mumbo_score(void *score_data, int size));
RECOMP_IMPORT(".", void native_send_honeycomb_collected(int world, int honeycomb_id, int xy_packed, int z));
RECOMP_IMPORT(".", void native_send_mumbo_token_collected(int world, int token_id, int xy_packed, int z));
RECOMP_IMPORT(".", unsigned int GetClockMS(void));

extern enum map_e map_get(void);
extern s32 level_get(void);

extern void fileProgressFlag_getSizeAndPtr(s32 *size, u8 **addr);

#ifndef COOP_DEBUG_LOGS
#define COOP_DEBUG_LOGS 0
#endif

static enum map_e s_last_map = 0;
static int s_frames_in_current_map = 0;
static const s32 MIN_FRAMES_BEFORE_NETWORK = 10;

// Pending teleport state (non-static so message_queue.c can access)
int s_pending_teleport = 0;
enum map_e s_pending_teleport_map = 0;
enum level_e s_pending_teleport_level = 0;
f32 s_pending_teleport_position[3] = {0.0f, 0.0f, 0.0f};
f32 s_pending_teleport_yaw = 0.0f;

static int is_real_map(enum map_e map);

int coop_network_is_safe_now(enum map_e map)
{
    if (!is_real_map(map))
    {
        return 0;
    }

    if (s_frames_in_current_map < MIN_FRAMES_BEFORE_NETWORK)
    {
        return 0;
    }

    return 1;
}

static int s_need_initial_upload = 0;

static int s_need_connect = 0;

void coop_mark_need_initial_upload(void)
{
    s_need_initial_upload = 1;
}

void coop_clear_need_initial_upload(void)
{
    s_need_initial_upload = 0;
}

static int is_real_map(enum map_e map)
{
    return (map > 0 && map <= 0x90);
}

static void coop_try_connect_if_ready(void)
{
    if (!s_need_connect)
    {
        return;
    }

    enum map_e current_map = map_get();
    if (!is_real_map(current_map))
    {
        return;
    }

    {
        char *skip = recomp_get_config_string("coop_skip_connect");
        if (!skip || skip[0] == '1')
        {
            toast_info("Co-op: connect skipped (isolation)");
            s_need_connect = 0;
            return;
        }
    }

    char *host = recomp_get_config_string("server_url");
    char *username = recomp_get_config_string("username");
    char *lobby_name = recomp_get_config_string("lobby_name");
    char *password = recomp_get_config_string("lobby_password");

    if (!host || !username || !lobby_name)
    {
        toast_error("Missing connection settings! Check config.");
        s_need_connect = 0;
        return;
    }

    if (host[0] == '\0' || username[0] == '\0')
    {
        toast_error("Empty server_url/username in config");
        s_need_connect = 0;
        return;
    }

    if (!password)
    {
        password = "";
    }

    toast_info("Co-op: connecting...");
    console_log_info("Co-op: connecting...");
    native_connect_to_server(host, username, lobby_name, password);

    s_need_connect = 0;
}

void send_ability_progress_blob(void)
{
    extern void ability_getSizeAndPtr(s32 * sizeOut, void **ptrOut);
    void *ptr = NULL;
    s32 size = 0;
    ability_getSizeAndPtr(&size, &ptr);

    if (ptr != NULL && size > 0)
    {
        native_send_ability_progress(ptr, size);
    }
}
int send_ability_progress_blob_cmd(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (applying_remote_state())
    {
        return 1;
    }

    send_ability_progress_blob();
    console_log_info("Sent ability progress blob");

    return 1;
}

void send_honeycomb_score_blob(void)
{
    extern void honeycombscore_getSizeAndPtr(s32 * sizeOut, void **ptrOut);
    void *ptr = NULL;
    s32 size = 0;
    honeycombscore_getSizeAndPtr(&size, &ptr);

    if (ptr != NULL && size > 0)
    {
        native_send_honeycomb_score(ptr, size);
    }
}

int send_honeycomb_score_blob_cmd(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (applying_remote_state())
    {
        return 1;
    }

    send_honeycomb_score_blob();
    console_log_info("Sent Honeycomb score blob");

    return 1;
}

void send_mumbo_score_blob(void)
{
    extern void mumboscore_getSizeAndPtr(s32 * sizeOut, void **ptrOut);
    void *ptr = NULL;
    s32 size = 0;
    mumboscore_getSizeAndPtr(&size, &ptr);

    if (ptr != NULL && size > 0)
    {
        native_send_mumbo_score(ptr, size);
    }
}

int send_mumbo_score_blob_cmd(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (applying_remote_state())
    {
        return 1;
    }

    send_mumbo_score_blob();
    console_log_info("Sent Mumbo score blob");

    return 1;
}

RECOMP_HOOK("transitionToMap")
void coop_on_transition_to_map(enum map_e map, s32 exit, s32 transition)
{
    (void)exit;
    (void)transition;

    if (COOP_DEBUG_LOGS)
    {
        recomp_printf("[COOP] hook: transitionToMap map=%d\n", (int)map);
    }

    if (s_need_initial_upload && coop_network_is_safe_now(map))
    {
        toast_info("Uploading save state...");
        native_upload_initial_save_data();

        {
            u8 *ptr = NULL;
            int size = 0;
            fileProgressFlag_getSizeAndPtr(&size, &ptr);
            if (ptr != NULL && size > 0)
            {
                native_send_file_progress_flags(ptr, size);
            }
        }

        send_ability_progress_blob();
        send_honeycomb_score_blob();
        send_mumbo_score_blob();

        s_need_initial_upload = 0;
    }
}

// RECOMP_HOOK_RETURN("transitionToMap")
// void coop_on_transition_to_map_return(enum map_e map, s32 exit, s32 transition)
// {
//     (void)map;
//     (void)exit;
//     (void)transition;

// }

RECOMP_HOOK_RETURN("ability_setLearned")
void ability_setLearned_hook(enum ability_e ability, bool val)
{
    if (applying_remote_state())
    {
        return;
    }

    send_ability_progress_blob();
}

RECOMP_HOOK_RETURN("honeycombscore_set")
void honeycombscore_set_hook(enum honeycomb_e indx, bool val)
{
    if (applying_remote_state())
    {
        return;
    }

    enum map_e mapId = map_get();
    if (sync_is_honeycomb_collected((s16)mapId, (s16)indx))
    {
        return;
    }

    send_honeycomb_score_blob();

    if (val)
    {
        float pos[3];
        player_getPosition(pos);

        s16 x = (s16)pos[0];
        s16 y = (s16)pos[1];
        s16 z = (s16)pos[2];

        sync_add_honeycomb((int)mapId, (int)indx, x, y, z);

        int xyPacked = (((int)(u16)y) << 16) | ((u16)x);
        native_send_honeycomb_collected((int)mapId, (int)indx, xyPacked, (int)z);
    }
}

RECOMP_HOOK_RETURN("mumboscore_set")
void mumboscore_set_hook(enum mumbotoken_e indx, bool val)
{
    if (applying_remote_state())
    {
        return;
    }

    enum map_e mapId = map_get();
    if (sync_is_token_collected((s16)mapId, (s16)indx))
    {
        return;
    }

    send_mumbo_score_blob();

    if (val)
    {
        float pos[3];
        player_getPosition(pos);

        s16 x = (s16)pos[0];
        s16 y = (s16)pos[1];
        s16 z = (s16)pos[2];

        sync_add_token((int)mapId, (int)indx, x, y, z);

        int xyPacked = (((int)(u16)y) << 16) | ((u16)x);
        native_send_mumbo_token_collected((int)mapId, (int)indx, xyPacked, (int)z);
    }
}

RECOMP_HOOK_RETURN("mainLoop")
void mainLoop(void)
{
    static const int COOP_MAINLOOP_STAGE = 5;

    static int s_mainloop_heartbeat = 0;
    if (COOP_DEBUG_LOGS && ((s_mainloop_heartbeat++ % 60) == 0))
    {
        recomp_printf("[COOP] mainLoop: tick\n");
    }

    toast_update();

    if (COOP_MAINLOOP_STAGE >= 1)
    {
        coop_try_connect_if_ready();
    }

    enum map_e current_map = map_get();
    enum level_e current_level = level_get();

    if (current_map != s_last_map)
    {
        s_last_map = current_map;
        s_frames_in_current_map = 0;
    }
    else
    {
        s_frames_in_current_map++;

        if (s_frames_in_current_map == 30)
        {
            despawn_collected_items_in_map();
        }
    }

    if (s_pending_teleport)
    {
        if (current_map == s_pending_teleport_map && current_level == s_pending_teleport_level)
        {
            extern void player_setPosition(f32 position[3]);
            player_setPosition(s_pending_teleport_position);

            recomp_printf("[COOP] Pending teleport applied: pos=(%.1f,%.1f,%.1f)\n",
                          s_pending_teleport_position[0], s_pending_teleport_position[1], s_pending_teleport_position[2]);

            toast_success("Teleported to player");
            s_pending_teleport = 0;
        }
        else if (COOP_DEBUG_LOGS)
        {
            recomp_printf("[COOP] Waiting for map transition: current=%d/%d, target=%d/%d\n",
                          current_map, current_level, s_pending_teleport_map, s_pending_teleport_level);
        }
    }

    if (COOP_MAINLOOP_STAGE >= 4)
    {
        if (current_map > 0 && current_map <= 0x90 && current_level >= 0 && current_level <= 20)
        {
            const s32 MIN_FRAMES_BEFORE_NETWORK_HEAVY = 60;

            if (s_frames_in_current_map >= MIN_FRAMES_BEFORE_NETWORK_HEAVY)
            {
                static int s_net_update_breadcrumb_throttle = 0;
                if (COOP_DEBUG_LOGS && ((s_net_update_breadcrumb_throttle++ % 60) == 0))
                {
                    recomp_printf("[COOP] update: before native_update_network\n");
                }

                native_update_network();

                if (COOP_DEBUG_LOGS && ((s_net_update_breadcrumb_throttle % 60) == 0))
                {
                    recomp_printf("[COOP] update: after native_update_network\n");
                }
            }
        }
    }

    if (COOP_MAINLOOP_STAGE >= 3)
    {
        if (s_frames_in_current_map >= MIN_FRAMES_BEFORE_NETWORK)
        {
            puppet_send_local_state();
        }

        puppet_update_all();
        player_list_ui_update();
        console_update();
        native_poll_console_input();
    }

    if (COOP_MAINLOOP_STAGE >= 4)
    {
        const s32 MIN_FRAMES_BEFORE_NETWORK_HEAVY = 60;
        if (s_frames_in_current_map < MIN_FRAMES_BEFORE_NETWORK_HEAVY)
        {
            return;
        }

        static GameMessage msg;
        int messagesProcessed = 0;
        const int MAX_MESSAGES_PER_FRAME = 100;

        while (messagesProcessed < MAX_MESSAGES_PER_FRAME && poll_queue_message(&msg))
        {
            process_queue_message(&msg);
            messagesProcessed++;
        }
    }
}

RECOMP_CALLBACK("*", recomp_on_init)
void on_init(void)
{
    ui_context_init();

    toast_init();
    player_list_init();
    player_list_ui_init();
    console_init();

    console_register_command("send_mumbo_score_blob", send_mumbo_score_blob_cmd, "Send current level mumbo token collection blob");
    console_register_command("send_honeycomb_score_blob", send_honeycomb_score_blob_cmd, "Send current level honeycomb collection blob");
    console_register_command("send_ability_progress_blob", send_ability_progress_blob_cmd, "Send current ability progress blob");

    if (!bkrecomp_note_saving_enabled())
    {
        toast_error("CO-OP MOD NOT LOADED - You must enable note saving in settings to use this mod.");
        return;
    }

    sync_init();

    char *host = recomp_get_config_string("server_url");
    char *username = recomp_get_config_string("username");
    char *lobby_name = recomp_get_config_string("lobby_name");
    char *password = recomp_get_config_string("lobby_password");

    if (!host || !username || !lobby_name)
    {
        toast_error("Missing connection settings! Check config.");
        return;
    }

    if (host[0] == '\0' || username[0] == '\0')
    {
        toast_error("Empty server_url/username in config");
        return;
    }

    if (!password)
    {
        password = "";
    }

    toast_info("Co-op: connect deferred until after map load");
    s_need_connect = 1;

    (void)0;
}

RECOMP_HOOK_RETURN("jiggyscore_setCollected")
void jiggyscore_setCollected_hook(int jiggy_enum_id, int collected_value)
{
    if (applying_remote_state())
    {
        return;
    }

    sync_add_jiggy(jiggy_enum_id, collected_value);
    native_sync_jiggy(jiggy_enum_id, collected_value);
}

RECOMP_CALLBACK("*", bkrecomp_note_collected_event)
void on_note_collected(enum map_e map_id, enum level_e level_id, u8 note_index)
{
    recomp_printf("[COOP] on_note_collected: map=%d, level=%d, note_index=%d\n", map_id, level_id, note_index);

    if (applying_remote_state())
    {
        recomp_printf("[COOP] on_note_collected: skipping (applying_remote_state=TRUE)\n");
        return;
    }

    recomp_printf("[COOP] on_note_collected: syncing note\n");
    sync_add_note(map_id, level_id, FALSE, note_index);
    native_sync_note(map_id, level_id, FALSE, note_index);
}

RECOMP_CALLBACK("*", bkrecomp_dynamic_note_collected_event)
void on_dynamic_note_collected(enum map_e map_id, enum level_e level_id)
{
    recomp_printf("[COOP] on_dynamic_note_collected: map=%d, level=%d\n", map_id, level_id);

    if (applying_remote_state())
    {
        recomp_printf("[COOP] on_dynamic_note_collected: skipping (applying_remote_state=TRUE)\n");
        return;
    }

    int dynamic_count = bkrecomp_dynamic_note_collected_count(map_id);
    recomp_printf("[COOP] on_dynamic_note_collected: syncing dynamic note (count=%d, index=%d)\n",
                  dynamic_count, dynamic_count - 1);
    sync_add_note(map_id, level_id, TRUE, dynamic_count - 1);
    native_sync_note(map_id, level_id, TRUE, (dynamic_count - 1));
}

static const int JIGSAW_COSTS[9] = {1, 2, 5, 7, 8, 9, 10, 12, 15};

RECOMP_HOOK_RETURN("fileProgressFlag_set")
void fileProgressFlag_set_hook(enum file_progress_e flag, s32 value)
{
    if (applying_remote_state())
    {
        return;
    }

    enum map_e map = map_get();
    if (!coop_network_is_safe_now(map))
    {
        return;
    }

    if (s_frames_in_current_map < 120)
    {
        return;
    }

    if (COOP_DEBUG_LOGS)
    {
        recomp_printf("[COOP] hook: fileProgressFlag_set flag=%d val=%d\n", (int)flag, (int)value);
    }

    static u32 s_last_file_progress_send = 0;
    const u32 FILE_PROGRESS_THROTTLE_MS = 2000;

    if (!applying_remote_state())
    {
        u32 now = GetClockMS();
        if (now - s_last_file_progress_send >= FILE_PROGRESS_THROTTLE_MS)
        {
            s_last_file_progress_send = now;

            u8 *ptr = NULL;
            int size = 0;
            fileProgressFlag_getSizeAndPtr(&size, &ptr);
            if (ptr != NULL && size > 0)
            {
                native_send_file_progress_flags(ptr, size);
            }
        }
    }

    if (flag >= FILEPROG_31_MM_OPEN && flag <= FILEPROG_39_CCW_OPEN && value == TRUE)
    {
        if (!applying_remote_state())
        {
            int world_id = (flag - FILEPROG_31_MM_OPEN) + 1;
            int jiggy_cost = JIGSAW_COSTS[world_id - 1];
            native_send_level_opened(world_id, jiggy_cost);
        }
    }
}
