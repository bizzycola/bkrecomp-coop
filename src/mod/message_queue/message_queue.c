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

extern ActorMarker *func_8032B16C(enum jiggy_e jiggy_id);
extern Actor *marker_getActor(ActorMarker *marker);
extern void marker_despawn(ActorMarker *marker);

static float dist_sq_f3(float ax, float ay, float az, float bx, float by, float bz)
{
    float dx = ax - bx;
    float dy = ay - by;
    float dz = az - bz;
    return (dx * dx) + (dy * dy) + (dz * dz);
}

static void despawn_nearest_actor_by_id(int actor_id, float x, float y, float z)
{
    extern Actor *actorArray_findActorFromActorId(enum actor_e actor_id);

    Actor *closest = actorArray_findActorFromActorId((enum actor_e)actor_id);
    float best_d2 = 0.0f;
    if (closest != NULL)
    {
        best_d2 = dist_sq_f3(closest->position[0], closest->position[1], closest->position[2], x, y, z);
    }

    const float max_d2 = 250.0f * 250.0f;
    if (closest != NULL && best_d2 <= max_d2 && closest->marker != NULL)
    {
        marker_despawn(closest->marker);
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
        break;
    }

    case MSG_PLAYER_CONNECTED:
    {
        const char *username = message_queue_get_string(msg);

        if (username != NULL)
        {
            toast_success(username);
        }
        else
        {
            toast_success("Player connected");
        }

        puppet_handle_player_connected(msg->playerId);
        break;
    }

    case MSG_PLAYER_DISCONNECTED:
    {
        toast_info("Player disconnected");
        puppet_handle_player_disconnected(msg->playerId);
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

        if (!sync_is_note_collected(mapId, levelId, isDynamic, noteIndex))
        {
            sync_add_note(mapId, levelId, isDynamic, noteIndex);
            collect_note(mapId, levelId, isDynamic, noteIndex);
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

        toast_info("Honeycomb collected (net)");

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

        toast_info("Mumbo token collected (net)");

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
            toast_info(statusBuf);
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

    default:
        break;
    }
}
