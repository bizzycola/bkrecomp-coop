#include "handlers/player_handlers.h"
#include "../message_queue/message_queue.h"
#include "modding.h"
#include "functions.h"
#include "variables.h"
#include "recomputils.h"
#include "../toast/toast.h"
#include "../puppets/puppet.h"
#include "../console/console.h"
#include "teleport/coop_teleport.h"

RECOMP_IMPORT(".", void native_send_player_info_response(u32 target_player_id, s16 map_id, s16 level_id, f32 *pos_and_yaw));

extern void player_list_add_player(u32 player_id, const char *username);
extern void player_list_remove_player(u32 player_id);
extern void player_list_ui_rebuild(void);

void handle_player_connected(const void *vmsg)
{
    const GameMessage *msg = (const GameMessage *)vmsg;
    const char *username = message_queue_get_string(msg);

    if (username != NULL)
    {
        char connectMsg[128];
        int i = 0;

        while (username[i] != '\0' && i < 100)
        {
            connectMsg[i] = username[i];
            i++;
        }

        const char *suffix = " connected";
        int j = 0;
        while (suffix[j] != '\0' && i < 127)
        {
            connectMsg[i++] = suffix[j++];
        }
        connectMsg[i] = '\0';

        toast_success(connectMsg);
        console_log_info(connectMsg);
    }
    else
    {
        toast_success("Player connected");
        console_log_info("Player connected (username not available)");
    }

    puppet_handle_player_connected(msg->playerId);
}

void handle_player_disconnected(const void *vmsg)
{
    const GameMessage *msg = (const GameMessage *)vmsg;
    recomp_printf("[MSG] PlayerDisconnected: ID=%d\n", msg->playerId);
    puppet_handle_player_disconnected(msg->playerId);

    player_list_remove_player(msg->playerId);
    player_list_ui_rebuild();
}

void handle_player_list_update(const void *vmsg)
{
    const GameMessage *msg = (const GameMessage *)vmsg;
    u32 player_id = (u32)msg->playerId;
    char username[MAX_MESSAGE_DATA_SIZE + 1];
    int n = (int)msg->dataSize;
    if (n < 0)
        n = 0;
    if (n > MAX_MESSAGE_DATA_SIZE)
        n = MAX_MESSAGE_DATA_SIZE;

    memcpy(username, msg->data, (size_t)n);
    username[n] = '\0';

    recomp_printf("[MSG] PlayerListUpdate: ID=%u, Username=%s\n", player_id, username);

    player_list_add_player(player_id, username);
    player_list_ui_rebuild();
}

void handle_player_info_request(const void *vmsg)
{
    const GameMessage *msg = (const GameMessage *)vmsg;
    u32 target_id = (u32)msg->param1;
    u32 requester_id = (u32)msg->param2;

    recomp_printf("[MSG] PlayerInfoRequest: target=%u, requester=%u\n", target_id, requester_id);

    extern enum map_e map_get(void);
    extern s32 level_get(void);
    extern void player_getPosition(f32 dst[3]);
    extern f32 player_getYaw(void);

    enum map_e current_map = map_get();
    enum level_e current_level = (enum level_e)level_get();
    f32 position[3];
    player_getPosition(position);
    f32 yaw = player_getYaw();

    recomp_printf("[MSG] Responding with: map=%d, level=%d, pos=(%.1f,%.1f,%.1f), yaw=%.1f\n",
                  current_map, current_level, position[0], position[1], position[2], yaw);

    f32 pos_and_yaw[4] = {position[0], position[1], position[2], yaw};

    native_send_player_info_response(requester_id, (s16)current_map, (s16)current_level, pos_and_yaw);
}

void handle_player_info_response(const void *vmsg)
{
    const GameMessage *msg = (const GameMessage *)vmsg;
    u32 target_player_id = (u32)msg->param1;
    s16 remote_map = (s16)msg->param2;
    s16 remote_level = (s16)msg->param3;

    f32 x, y, z, yaw;
    memcpy(&x, &msg->param4, sizeof(f32));
    memcpy(&y, &msg->param5, sizeof(f32));
    memcpy(&z, &msg->param6, sizeof(f32));
    memcpy(&yaw, &msg->paramF1, sizeof(f32));

    recomp_printf("[MSG] PlayerInfoResponse: target=%u, map=%d, level=%d, pos=(%.1f,%.1f,%.1f), yaw=%.1f\n",
                  target_player_id, remote_map, remote_level, x, y, z, yaw);

    extern enum map_e map_get(void);
    extern s32 level_get(void);
    extern void player_setPosition(f32 position[3]);
    extern void transitionToMap(enum map_e map, s32 exit, s32 transition);

    enum map_e current_map = map_get();
    enum level_e current_level = (enum level_e)level_get();

    if (current_map == (enum map_e)remote_map && current_level == (enum level_e)remote_level)
    {
        f32 position[3] = {x, y, z};
        player_setPosition(position);
        recomp_printf("[MSG] Teleported to player in same map/level\n");
        toast_success("Teleported to player");
    }
    else
    {
        recomp_printf("[MSG] Storing pending teleport and transitioning to map %d\n", remote_map);

        f32 position[3] = {x, y, z};
        coop_teleport_set_pending((enum map_e)remote_map, (enum level_e)remote_level, position, yaw);

        transitionToMap((enum map_e)remote_map, 0, 0);
        toast_info("Transitioning to player's location...");
    }
}
