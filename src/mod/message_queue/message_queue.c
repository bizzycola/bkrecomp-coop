#include "message_queue.h"
#include "modding.h"
#include "functions.h"
#include "variables.h"
#include "../toast/toast.h"
#include "recomputils.h"
#include "../sync/sync.h"
#include "../collection/collection.h"
#include "../puppets/puppet.h"
#include "bkrecomp_api.h"
#include "../include/mod/console/console.h"

RECOMP_IMPORT(".", void native_send_player_info_response(u32 target_player_id, s16 map_id, s16 level_id, f32 *pos_and_yaw));

extern ActorMarker *func_8032B16C(enum jiggy_e jiggy_id);
extern Actor *marker_getActor(ActorMarker *marker);
extern void marker_despawn(ActorMarker *marker);

typedef struct actor_array ActorArray;
extern ActorArray *suBaddieActorArray;

#define POS_TOLERANCE 10
static int positions_match(s16 x1, s16 y1, s16 z1, s16 x2, s16 y2, s16 z2)
{
    int dx = x1 - x2;
    int dy = y1 - y2;
    int dz = z1 - z2;

    if (dx < 0)
        dx = -dx;
    if (dy < 0)
        dy = -dy;
    if (dz < 0)
        dz = -dz;

    return (dx <= POS_TOLERANCE && dy <= POS_TOLERANCE && dz <= POS_TOLERANCE);
}

static void despawn_nearest_actor_by_id(int actor_id, float x, float y, float z)
{
    if (suBaddieActorArray == NULL)
    {
        return;
    }

    s16 target_x = (s16)x;
    s16 target_y = (s16)y;
    s16 target_z = (s16)z;

    Actor *begin = suBaddieActorArray->data;
    Actor *end = begin + suBaddieActorArray->cnt;

    for (Actor *actor = begin; actor < end; actor++)
    {
        if (actor->despawn_flag || actor->marker == NULL)
        {
            continue;
        }

        if ((enum actor_e)actor_id == actor->modelCacheIndex)
        {
            s16 actor_x = (s16)actor->position[0];
            s16 actor_y = (s16)actor->position[1];
            s16 actor_z = (s16)actor->position[2];

            if (positions_match(actor_x, actor_y, actor_z, target_x, target_y, target_z))
            {
                marker_despawn(actor->marker);
                break;
            }
        }
    }
}

typedef struct
{
    int kind;
    int id;
} ApplyScoreCtx;

static void apply_score_set_fn(void *vctx)
{
    ApplyScoreCtx *ctx = (ApplyScoreCtx *)vctx;
    if (!ctx)
        return;

    if (ctx->kind == 0)
    {
        extern void honeycombscore_set(enum honeycomb_e indx, bool val);
        honeycombscore_set((enum honeycomb_e)ctx->id, TRUE);
    }
    else
    {
        extern void mumboscore_set(enum mumbotoken_e indx, bool val);
        mumboscore_set((enum mumbotoken_e)ctx->id, TRUE);
    }
}

static int note_level_slot_to_level_id(int slot)
{
    switch (slot)
    {
    case 0:
        return LEVEL_1_MUMBOS_MOUNTAIN;
    case 1:
        return LEVEL_2_TREASURE_TROVE_COVE;
    case 2:
        return LEVEL_3_CLANKERS_CAVERN;
    case 3:
        return LEVEL_4_BUBBLEGLOOP_SWAMP;
    case 4:
        return LEVEL_5_FREEZEEZY_PEAK;
    case 5:
        return LEVEL_7_GOBIS_VALLEY;
    case 6:
        return LEVEL_8_CLICK_CLOCK_WOOD;
    case 7:
        return LEVEL_9_RUSTY_BUCKET_BAY;
    case 8:
        return LEVEL_A_MAD_MONSTER_MANSION;
    default:
        return 0;
    }
}

typedef struct ApplyFpCtx
{
    const GameMessage *msg;
    int byteCount;
} ApplyFpCtx;

typedef struct ApplyBlobBytesCtx
{
    const u8 *bytes;
    int size;
} ApplyBlobBytesCtx;

static void apply_file_progress_flags_fn(void *vctx)
{
    ApplyFpCtx *ctx = (ApplyFpCtx *)vctx;
    if (!ctx || !ctx->msg)
        return;

    const GameMessage *msg = ctx->msg;
    int byteCount = ctx->byteCount;
    if (byteCount <= 0)
        return;

    for (int byteIndex = 0; byteIndex < byteCount; byteIndex++)
    {
        u8 b = msg->data[byteIndex];
        if (b == 0)
            continue;
        for (int bit = 0; bit < 8; bit++)
        {
            if (b & (1 << bit))
            {
                int flagIndex = (byteIndex * 8) + bit;
                fileProgressFlag_setN((enum file_progress_e)flagIndex, TRUE, 0);
            }
        }
    }
}

static void apply_ability_progress_fn(void *vctx)
{
    ApplyBlobBytesCtx *ctx = (ApplyBlobBytesCtx *)vctx;
    if (!ctx || !ctx->bytes || ctx->size <= 0)
        return;

    extern void ability_setLearned(enum ability_e ability, bool hasLearned);

    for (int i = 0; i < ctx->size; i++)
    {
        u8 b = ctx->bytes[i];
        if (b == 0)
            continue;

        for (int bit = 0; bit < 8; bit++)
        {
            if (b & (1 << bit))
            {
                int idx = (i * 8) + bit;
                ability_setLearned((enum ability_e)idx, TRUE);
            }
        }
    }
}

static void apply_honeycomb_score_fn(void *vctx)
{
    ApplyBlobBytesCtx *ctx = (ApplyBlobBytesCtx *)vctx;
    if (!ctx || !ctx->bytes || ctx->size <= 0)
        return;

    extern void honeycombscore_set(enum honeycomb_e indx, bool val);

    for (int i = 0; i < ctx->size; i++)
    {
        u8 b = ctx->bytes[i];
        if (b == 0)
            continue;
        for (int bit = 0; bit < 8; bit++)
        {
            if (b & (1 << bit))
            {
                int idx = (i * 8) + bit;
                honeycombscore_set((enum honeycomb_e)idx, TRUE);
            }
        }
    }
}

static void apply_mumbo_score_fn(void *vctx)
{
    ApplyBlobBytesCtx *ctx = (ApplyBlobBytesCtx *)vctx;
    if (!ctx || !ctx->bytes || ctx->size <= 0)
        return;

    extern void mumboscore_set(enum mumbotoken_e indx, bool val);

    for (int i = 0; i < ctx->size; i++)
    {
        u8 b = ctx->bytes[i];
        if (b == 0)
            continue;
        for (int bit = 0; bit < 8; bit++)
        {
            if (b & (1 << bit))
            {
                int idx = (i * 8) + bit;
                mumboscore_set((enum mumbotoken_e)idx, TRUE);
            }
        }
    }
}

RECOMP_IMPORT(".", int net_msg_poll(void *buffer));

static GameMessage s_poll_buf;

int poll_queue_message(GameMessage *out_message)
{
    if (!out_message)
    {
        return 0;
    }

    return net_msg_poll(out_message);
}

void process_queue_message(const GameMessage *msg)
{
    if (!msg)
        return;

    extern void coop_mark_need_initial_upload(void);

    switch (msg->type)
    {
    case MSG_INITIAL_SAVE_DATA_REQUEST:
    {
        coop_mark_need_initial_upload();

        extern enum map_e map_get(void);
        extern int coop_network_is_safe_now(enum map_e map);
        extern void native_upload_initial_save_data(void);
        extern void native_send_file_progress_flags(u8 * data, int size);
        extern void fileProgressFlag_getSizeAndPtr(s32 * size, u8 * *addr);
        extern void send_ability_progress_blob(void);
        extern void send_honeycomb_score_blob(void);
        extern void send_mumbo_score_blob(void);

        enum map_e current_map = map_get();
        if (coop_network_is_safe_now(current_map))
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

            extern void coop_clear_need_initial_upload(void);
            coop_clear_need_initial_upload();
        }
        break;
    }

    case MSG_PLAYER_CONNECTED:
    {
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
        break;
    }

    case MSG_PLAYER_DISCONNECTED:
    {
        recomp_printf("[MSG] PlayerDisconnected: ID=%d\n", msg->playerId);
        puppet_handle_player_disconnected(msg->playerId);

        extern void player_list_remove_player(u32 player_id);
        extern void player_list_ui_rebuild(void);

        player_list_remove_player(msg->playerId);
        player_list_ui_rebuild();
        break;
    }

    case MSG_JIGGY_COLLECTED:
    {
        int jiggyEnumId = msg->param1;
        int collectedValue = msg->param2;

        toast_info("Jiggy collected");

        if (!sync_is_jiggy_collected(jiggyEnumId, collectedValue))
        {
            sync_add_jiggy(jiggyEnumId, collectedValue);
            collect_jiggy(jiggyEnumId, collectedValue);
        }

        break;
    }

    case MSG_NOTE_COLLECTED:
    {
        int mapId = msg->param1;
        int levelId = msg->param2;
        int isDynamic = (msg->param3 != 0);
        int noteIndex = msg->param4;

        recomp_printf("[MSG_QUEUE] Processing NOTE_COLLECTED: map=%d, level=%d, is_dynamic=%d, note_index=%d\n",
                      mapId, levelId, isDynamic, noteIndex);

        if (!sync_is_note_collected(mapId, levelId, isDynamic, noteIndex))
        {
            recomp_printf("[MSG_QUEUE] Note not yet collected locally, adding and collecting\n");
            sync_add_note(mapId, levelId, isDynamic, noteIndex);
            collect_note(mapId, levelId, isDynamic, noteIndex);
        }
        else
        {
            recomp_printf("[MSG_QUEUE] Note already collected locally, skipping\n");
        }
        break;
    }

    case MSG_PUPPET_UPDATE:
    {
        int playerId = msg->playerId;

        float x = msg->paramF1;
        float y = msg->paramF2;
        float z = msg->paramF3;

        short mapId = (short)(msg->param4 & 0xFF);
        short levelId = (short)((msg->param4 >> 8) & 0xFF);
        unsigned short animId = (unsigned short)((msg->param4 >> 16) & 0xFFFF);

        float yaw, pitch, roll;
        memcpy(&yaw, &msg->param1, sizeof(float));
        memcpy(&pitch, &msg->param2, sizeof(float));
        memcpy(&roll, &msg->param3, sizeof(float));

        float anim_duration = msg->paramF4;
        if (anim_duration <= 0.0f)
        {
            anim_duration = 1.0f;
        }
        float anim_timer = msg->paramF5;

        u8 playback_type = (u8)msg->param5;
        u8 playback_direction = (u8)msg->param6;

        PuppetUpdateData data;
        data.x = x;
        data.y = y;
        data.z = z;
        data.yaw = yaw;
        data.pitch = pitch;
        data.roll = roll;
        data.map_id = mapId;
        data.level_id = levelId;
        data.anim_id = (u16)animId;
        data.anim_duration = anim_duration;
        data.anim_timer = anim_timer;
        data.playback_type = playback_type;
        data.playback_direction = playback_direction;
        data.model_id = 0;
        data.flags = 0;

        puppet_handle_remote_update(playerId, &data);
        break;
    }

    case MSG_LEVEL_OPENED:
    {
        int worldId = msg->param1;
        int jiggyCost = msg->param2;

        toast_info("Level opened");
        open_level(worldId, jiggyCost);
        break;
    }

    case MSG_NOTE_SAVE_DATA:
    {
        int levelSlot = msg->param1;
        int size = (int)msg->dataSize;
        int levelId = note_level_slot_to_level_id(levelSlot);

        if (levelId != 0 && size == 32 && bkrecomp_note_saving_active())
        {
            for (int byteIndex = 0; byteIndex < 32; byteIndex++)
            {
                u8 b = msg->data[byteIndex];
                if (b == 0)
                    continue;

                for (int bit = 0; bit < 8; bit++)
                {
                    if (b & (1 << bit))
                    {
                        int noteIndex = (byteIndex * 8) + bit;
                        bkrecomp_set_note_collected(0, (enum level_e)levelId, (u8)noteIndex);
                    }
                }
            }
        }
        break;
    }

    case MSG_FILE_PROGRESS_FLAGS:
    {
        int byteCount = msg->param1;
        if (byteCount < 0)
            byteCount = 0;
        if (byteCount > (int)msg->dataSize)
            byteCount = (int)msg->dataSize;

        if (byteCount > 0)
        {
            ApplyFpCtx ctx;
            ctx.msg = msg;
            ctx.byteCount = byteCount;

            with_applying_remote_state(apply_file_progress_flags_fn, &ctx);
        }
        break;
    }

    case MSG_ABILITY_PROGRESS:
    {
        int byteCount = msg->param1;
        if (byteCount < 0)
            byteCount = 0;
        if (byteCount > (int)msg->dataSize)
            byteCount = (int)msg->dataSize;

        if (byteCount > 0)
        {
            ApplyBlobBytesCtx ctx;
            ctx.bytes = (const u8 *)msg->data;
            ctx.size = byteCount;
            with_applying_remote_state(apply_ability_progress_fn, &ctx);
        }
        break;
    }

    case MSG_HONEYCOMB_SCORE:
    {
        int byteCount = msg->param1;
        if (byteCount < 0)
            byteCount = 0;
        if (byteCount > (int)msg->dataSize)
            byteCount = (int)msg->dataSize;

        if (byteCount > 0)
        {
            ApplyBlobBytesCtx ctx;
            ctx.bytes = (const u8 *)msg->data;
            ctx.size = byteCount;
            with_applying_remote_state(apply_honeycomb_score_fn, &ctx);
        }
        break;
    }

    case MSG_MUMBO_SCORE:
    {
        int byteCount = msg->param1;
        if (byteCount < 0)
            byteCount = 0;
        if (byteCount > (int)msg->dataSize)
            byteCount = (int)msg->dataSize;

        if (byteCount > 0)
        {
            ApplyBlobBytesCtx ctx;
            ctx.bytes = (const u8 *)msg->data;
            ctx.size = byteCount;
            with_applying_remote_state(apply_mumbo_score_fn, &ctx);
        }
        break;
    }

    case MSG_HONEYCOMB_COLLECTED:
    {
        int mapId = msg->param1;
        int honeycombId = msg->param2;
        s16 x = (s16)msg->param3;
        s16 y = (s16)msg->param4;
        s16 z = (s16)msg->param5;

        if (!sync_is_honeycomb_collected((s16)mapId, (s16)honeycombId))
        {
            sync_add_honeycomb(mapId, honeycombId, x, y, z);

            ApplyScoreCtx ctx;
            ctx.kind = 0;
            ctx.id = honeycombId;
            with_applying_remote_state(apply_score_set_fn, &ctx);

            despawn_nearest_actor_by_id(ACTOR_47_EMPTY_HONEYCOMB, (float)x, (float)y, (float)z);
            despawn_nearest_actor_by_id(ACTOR_50_HONEYCOMB, (float)x, (float)y, (float)z);
        }
        break;
    }

    case MSG_MUMBO_TOKEN_COLLECTED:
    {
        int mapId = msg->param1;
        int tokenId = msg->param2;
        s16 x = (s16)msg->param3;
        s16 y = (s16)msg->param4;
        s16 z = (s16)msg->param5;

        if (!sync_is_token_collected((s16)mapId, (s16)tokenId))
        {
            sync_add_token(mapId, tokenId, x, y, z);

            ApplyScoreCtx ctx;
            ctx.kind = 1;
            ctx.id = tokenId;
            with_applying_remote_state(apply_score_set_fn, &ctx);

            despawn_nearest_actor_by_id(ACTOR_2D_MUMBO_TOKEN, (float)x, (float)y, (float)z);
        }
        break;
    }

    case MSG_CONNECTION_STATUS:
    {
        char statusBuf[MAX_MESSAGE_DATA_SIZE + 1];
        int n = (int)msg->dataSize;
        if (n < 0)
            n = 0;
        if (n > MAX_MESSAGE_DATA_SIZE)
            n = MAX_MESSAGE_DATA_SIZE;

        memcpy(statusBuf, msg->data, (size_t)n);
        statusBuf[n] = '\0';

        if (statusBuf[0] != '\0')
        {
            toast_show_immediate_custom("Connected!", TOAST_DEFAULT_DURATION,
                                        TOAST_POS_TOP_RIGHT, TOAST_SIZE_MEDIUM, TOAST_STYLE_SUCCESS);

            extern void player_list_ui_show(void);
            player_list_ui_show();
        }
        break;
    }

    case MSG_CONNECTION_ERROR:
    {
        char errBuf[MAX_MESSAGE_DATA_SIZE + 1];
        int n = (int)msg->dataSize;
        if (n < 0)
            n = 0;
        if (n > MAX_MESSAGE_DATA_SIZE)
            n = MAX_MESSAGE_DATA_SIZE;

        memcpy(errBuf, msg->data, (size_t)n);
        errBuf[n] = '\0';

        if (errBuf[0] != '\0')
        {
            toast_error(errBuf);
        }
        break;
    }

    case MSG_PLAYER_INFO_REQUEST:
    {
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
        break;
    }

    case MSG_PLAYER_INFO_RESPONSE:
    {
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

        extern int s_pending_teleport;
        extern enum map_e s_pending_teleport_map;
        extern enum level_e s_pending_teleport_level;
        extern f32 s_pending_teleport_position[3];
        extern f32 s_pending_teleport_yaw;

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

            s_pending_teleport = 1;
            s_pending_teleport_map = (enum map_e)remote_map;
            s_pending_teleport_level = (enum level_e)remote_level;
            s_pending_teleport_position[0] = x;
            s_pending_teleport_position[1] = y;
            s_pending_teleport_position[2] = z;
            s_pending_teleport_yaw = yaw;

            transitionToMap((enum map_e)remote_map, 0, 0);
            toast_info("Transitioning to player's location...");
        }
        break;
    }

    case MSG_PLAYER_LIST_UPDATE:
    {
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

        extern void player_list_add_player(u32 player_id, const char *username);
        extern void player_list_ui_rebuild(void);

        player_list_add_player(player_id, username);
        player_list_ui_rebuild();
        break;
    }

    case MSG_CONSOLE_TOGGLE:
    {
        extern void console_toggle(void);
        console_toggle();
        break;
    }

    case MSG_CONSOLE_KEY:
    {
        extern void console_handle_key(char key);
        extern void console_handle_backspace(void);
        extern void console_handle_enter(void);

        char key = (char)msg->param1;

        if (key == '\b')
        {
            console_handle_backspace();
        }
        else if (key == '\r' || key == '\n')
        {
            console_handle_enter();
        }
        else if (key >= 32 && key < 127)
        {
            console_handle_key(key);
        }
        break;
    }

    default:
        break;
    }
}
