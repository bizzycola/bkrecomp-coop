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

RECOMP_IMPORT(".", int native_lib_test(int param1, float param2, const char *param3));
RECOMP_IMPORT(".", int native_connect_to_server(const char *host, const char *username, const char *lobby, const char *password));
RECOMP_IMPORT(".", int native_update_network(void));
RECOMP_IMPORT(".", int native_disconnect_from_server(void));
RECOMP_IMPORT(".", int native_sync_jiggy(int jiggyEnumId, int collectedValue));
RECOMP_IMPORT(".", int native_sync_note(int mapId, int levelId, bool isDynamic, int noteIndex));

RECOMP_HOOK_RETURN("mainLoop")
void mainLoop(void)
{
    toast_update();

    native_update_network();

    GameMessage msg;
    int messagesProcessed = 0;
    const int MAX_MESSAGES_PER_FRAME = 100;

    while (messagesProcessed < MAX_MESSAGES_PER_FRAME && poll_queue_message(&msg))
    {
        process_queue_message(&msg);
        messagesProcessed++;
    }
}

RECOMP_CALLBACK("*", recomp_on_init)
void on_init(void)
{
    toast_init();

    if(!bkrecomp_note_saving_enabled()) {
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

    if (!password)
    {
        password = "";
    }

    native_connect_to_server(host, username, lobby_name, password);
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
    if (applying_remote_state())
    {
        return;
    }

    sync_add_note(map_id, level_id, FALSE, note_index);
    native_sync_note(map_id, level_id, FALSE, note_index);
}

RECOMP_CALLBACK("*", bkrecomp_dynamic_note_collected_event)
void on_dynamic_note_collected(enum map_e map_id, enum level_e level_id)
{
    if (applying_remote_state())
    {
        return;
    }

    int dynamic_count = bkrecomp_dynamic_note_collected_count(map_id);
    sync_add_note(map_id, level_id, TRUE, dynamic_count - 1);
    native_sync_note(map_id, level_id, TRUE, dynamic_count - 1);
}
