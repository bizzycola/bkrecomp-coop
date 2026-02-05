#include "handlers/collection_handlers.h"
#include "../message_queue/message_queue.h"
#include "modding.h"
#include "functions.h"
#include "variables.h"
#include "../toast/toast.h"
#include "../sync/sync.h"
#include "../collection/collection.h"
#include "bkrecomp_api.h"
#include "util.h"

extern void marker_despawn(ActorMarker *marker);

typedef struct actor_array ActorArray;
extern ActorArray *suBaddieActorArray;

#define POS_TOLERANCE 10

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

            if (util_positions_match_tolerance(actor_x, actor_y, actor_z, target_x, target_y, target_z, POS_TOLERANCE))
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

void handle_jiggy_collected(const void *vmsg)
{
    const GameMessage *msg = (const GameMessage *)vmsg;
    int jiggyEnumId = msg->param1;
    int collectedValue = msg->param2;

    toast_info("Jiggy collected");

    if (!sync_is_jiggy_collected(jiggyEnumId, collectedValue))
    {
        sync_add_jiggy(jiggyEnumId, collectedValue);
        collect_jiggy(jiggyEnumId, collectedValue);
    }
}

void handle_note_collected(const void *vmsg)
{
    const GameMessage *msg = (const GameMessage *)vmsg;
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
}

void handle_honeycomb_collected(const void *vmsg)
{
    const GameMessage *msg = (const GameMessage *)vmsg;
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
}

void handle_mumbo_token_collected(const void *vmsg)
{
    const GameMessage *msg = (const GameMessage *)vmsg;
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
}
