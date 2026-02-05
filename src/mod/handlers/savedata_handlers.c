#include "handlers/savedata_handlers.h"
#include "../message_queue/message_queue.h"
#include "modding.h"
#include "functions.h"
#include "variables.h"
#include "../toast/toast.h"
#include "../collection/collection.h"
#include "bkrecomp_api.h"
#include "network/coop_network.h"
#include "blob/blob_sender.h"

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

void handle_initial_save_data_request(const void *vmsg)
{
    const GameMessage *msg = (const GameMessage *)vmsg;
    coop_mark_need_initial_upload();

    extern enum map_e map_get(void);
    extern int coop_network_is_safe_now(enum map_e map);

    enum map_e current_map = map_get();
    if (coop_network_is_safe_now(current_map))
    {
        toast_info("Uploading save state...");
        native_upload_initial_save_data();

        {
            extern void fileProgressFlag_getSizeAndPtr(s32 * size, u8 * *addr);
            u8 *ptr = NULL;
            s32 size = 0;
            fileProgressFlag_getSizeAndPtr(&size, &ptr);
            if (ptr != NULL && size > 0)
            {
                native_send_file_progress_flags(ptr, size);
            }
        }

        send_ability_progress_blob();
        send_honeycomb_score_blob();
        send_mumbo_score_blob();

        coop_clear_need_initial_upload();
    }
}

void handle_file_progress_flags(const void *vmsg)
{
    const GameMessage *msg = (const GameMessage *)vmsg;
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
}

void handle_ability_progress(const void *vmsg)
{
    const GameMessage *msg = (const GameMessage *)vmsg;
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
}

void handle_honeycomb_score(const void *vmsg)
{
    const GameMessage *msg = (const GameMessage *)vmsg;
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
}

void handle_mumbo_score(const void *vmsg)
{
    const GameMessage *msg = (const GameMessage *)vmsg;
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
}

void handle_note_save_data(const void *vmsg)
{
    const GameMessage *msg = (const GameMessage *)vmsg;
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
}
