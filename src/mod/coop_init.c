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
#include "message_queue/coop_messages.h"
#include "toast/toast.h"
#include "sync/sync.h"

RECOMP_IMPORT(".", int native_lib_test(int param1, float param2, const char *param3));
RECOMP_IMPORT(".", int native_connect_to_server(const char *host, const char *username, const char *lobby, const char *password));
RECOMP_IMPORT(".", int native_update_network(void));
RECOMP_IMPORT(".", int native_disconnect_from_server(void));

RECOMP_HOOK_RETURN("mainLoop")
void mainLoop(void)
{
    toast_update();

    native_update_network();

    GameMessage msg;
    int messagesProcessed = 0;
    const int MAX_MESSAGES_PER_FRAME = 100;

    while (messagesProcessed < MAX_MESSAGES_PER_FRAME && coop_poll_message(&msg))
    {
        coop_process_message(&msg);
        messagesProcessed++;
    }
}

RECOMP_CALLBACK("*", recomp_on_init)
void on_init(void)
{
    toast_init();
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
void jiggyscore_setCollected_hook(int level_id, int jiggy_id)
{
    sync_add_jiggy(level_id, jiggy_id);
}
