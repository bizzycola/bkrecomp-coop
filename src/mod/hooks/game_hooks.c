#include "hooks/game_hooks.h"
#include "modding.h"
#include "functions.h"
#include "recomputils.h"
#include "network/coop_network.h"
#include "blob/blob_sender.h"
#include "../message_queue/message_queue.h"
#include "../sync/sync.h"
#include "../toast/toast.h"
#include "../collection/collection.h"
#include "util.h"
#include "bkrecomp_api.h"

#ifndef COOP_DEBUG_LOGS
#define COOP_DEBUG_LOGS 0
#endif

extern enum map_e map_get(void);
extern s32 level_get(void);
extern void fileProgressFlag_getSizeAndPtr(s32 *size, u8 **addr);
extern void player_getPosition(f32 position[3]);

extern int coop_network_is_safe_for_map(enum map_e map, int frames_in_map);

static const int JIGSAW_COSTS[9] = {1, 2, 5, 7, 8, 9, 10, 12, 15};

RECOMP_HOOK("transitionToMap")
void coop_on_transition_to_map(enum map_e map, s32 exit, s32 transition)
{
    (void)exit;
    (void)transition;

    if (COOP_DEBUG_LOGS)
    {
        recomp_printf("[COOP] hook: transitionToMap map=%d\n", (int)map);
    }

    if (coop_needs_initial_upload() && coop_network_is_safe_for_map(map, 60))
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

        coop_clear_need_initial_upload();
    }
}

RECOMP_HOOK_RETURN("ability_setLearned")
void ability_setLearned_hook(enum ability_e ability, bool val)
{
    (void)ability;
    (void)val;

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

        int xyPacked = util_pack_xy_coords(x, y);
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

        int xyPacked = util_pack_xy_coords(x, y);
        native_send_mumbo_token_collected((int)mapId, (int)indx, xyPacked, (int)z);
    }
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

void game_hooks_init(void)
{
}
