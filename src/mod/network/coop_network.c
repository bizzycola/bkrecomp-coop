#include "network/coop_network.h"
#include "recompconfig.h"
#include "../toast/toast.h"
#include "console/console.h"

#ifndef COOP_DEBUG_LOGS
#define COOP_DEBUG_LOGS 0
#endif

static const s32 MIN_FRAMES_BEFORE_NETWORK = 10;

static int s_need_initial_upload = 0;
static int s_need_connect = 0;

static int is_real_map(enum map_e map)
{
    return (map > 0 && map <= 0x90);
}

int coop_network_is_safe_now(enum map_e map)
{
    (void)map;

    return 1;
}

void coop_mark_need_initial_upload(void)
{
    s_need_initial_upload = 1;
}

void coop_clear_need_initial_upload(void)
{
    s_need_initial_upload = 0;
}

int coop_needs_initial_upload(void)
{
    return s_need_initial_upload;
}

void coop_mark_need_connect(void)
{
    s_need_connect = 1;
}

void coop_try_connect_if_ready(enum map_e current_map, int frames_in_map)
{
    if (!s_need_connect)
    {
        return;
    }

    if (!is_real_map(current_map))
    {
        return;
    }

    if (frames_in_map < MIN_FRAMES_BEFORE_NETWORK)
    {
        return;
    }

    {
        char *skip = recomp_get_config_string("coop_skip_connect");
        if (!skip || skip[0] == '1')
        {
            toast_info("Co-op: connect skipped (isolation)");
            console_log_info("Co-op: connect skipped (isolation)");
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
