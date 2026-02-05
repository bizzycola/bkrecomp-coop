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
#include "util.h"
#include "network/coop_network.h"
#include "teleport/coop_teleport.h"
#include "blob/blob_sender.h"
#include "hooks/game_hooks.h"

extern enum map_e map_get(void);
extern s32 level_get(void);

#ifndef COOP_DEBUG_LOGS
#define COOP_DEBUG_LOGS 0
#endif

static enum map_e s_last_map = 0;
static int s_frames_in_current_map = 0;
static const s32 MIN_FRAMES_BEFORE_NETWORK = 10;

static int is_real_map(enum map_e map)
{
    return (map > 0 && map <= 0x90);
}

int coop_network_is_safe_for_map(enum map_e map, int min_frames)
{
    if (!is_real_map(map))
    {
        return 0;
    }

    if (s_frames_in_current_map < min_frames)
    {
        return 0;
    }

    return 1;
}

// hooks the games main update loop
// does a lot of stuff (network polling, UI updates, etc)
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

    // connect once banjo has entered a real map
    // basically don't connect in file select
    if (COOP_MAINLOOP_STAGE >= 1)
    {
        enum map_e current_map = map_get();
        coop_try_connect_if_ready(current_map, s_frames_in_current_map);
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

    // teleport stuff
    coop_teleport_update(current_map, current_level);

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

// called when the game itself is first started / initialised
// (when you hit play game in the menu)
RECOMP_CALLBACK("*", recomp_on_init)
void on_init(void)
{
    // init all the things
    ui_context_init();

    toast_init();
    player_list_init();
    player_list_ui_init();
    console_init();
    game_hooks_init();

    // register some commands for send scores
    console_register_command("send_mumbo_score_blob", send_mumbo_score_blob_cmd, "Send current level mumbo token collection blob");
    console_register_command("send_honeycomb_score_blob", send_honeycomb_score_blob_cmd, "Send current level honeycomb collection blob");
    console_register_command("send_ability_progress_blob", send_ability_progress_blob_cmd, "Send current ability progress blob");

    if (!bkrecomp_note_saving_enabled())
    {
        toast_error("CO-OP MOD NOT LOADED - You must enable note saving in settings to use this mod.");
        return;
    }

    sync_init();

    // handle connections
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
    coop_mark_need_connect();
    coop_mark_need_initial_upload();

    (void)0;
}
