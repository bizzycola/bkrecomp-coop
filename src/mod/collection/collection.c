#include "collection.h"
#include "bkrecomp_api.h"
#include "../sync/sync.h"

typedef struct actor_array ActorArray;
extern ActorArray *suBaddieActorArray;

static bool apply_remote_state = FALSE;

extern void jiggyscore_setCollected(int levelid, int jiggy_id);
extern int jiggyscore_isCollected(int levelid, int jiggy_id);
extern void item_adjustByDiffWithoutHud(int item, int diff);
extern ActorMarker *func_8032B16C(enum jiggy_e jiggy_id);
extern void marker_despawn(ActorMarker *marker);
extern void chjiggy_hide(Actor *actor);
extern Actor *marker_getActor(ActorMarker *marker);
extern enum map_e map_get(void);
extern enum level_e level_get(void);
extern void transitionToMap(enum map_e map, s32 exit, s32 transition);
extern s32 fileProgressFlag_get(enum file_progress_e flag);
extern void fileProgressFlag_set(enum file_progress_e flag, s32 value);
extern Actor *actorArray_findActorFromActorId(enum actor_e actor_id);

extern struct
{
    Cube *cubes;
    f32 margin;
    s32 min[3];
    s32 max[3];
    s32 stride[2];
    s32 cubeCnt;
    s32 unk2C;
    s32 width[3];
    Cube *unk3C;
    Cube *unk40;
    s32 unk44;
} sCubeList;

bool applying_remote_state()
{
    return apply_remote_state;
}

void with_applying_remote_state(void (*fn)(void *ctx), void *ctx)
{
    if (!fn)
    {
        return;
    }

    apply_remote_state = TRUE;
    fn(ctx);
    apply_remote_state = FALSE;
}

void collect_jiggy(int jiggy_enum_id, int collected_value)
{
    bool already_collected = jiggyscore_isCollected(jiggy_enum_id, collected_value);

    if (!already_collected)
    {
        ActorMarker *jiggy_marker = func_8032B16C(jiggy_enum_id);

        apply_remote_state = TRUE;
        jiggyscore_setCollected(jiggy_enum_id, collected_value);
        apply_remote_state = FALSE;

        item_adjustByDiffWithoutHud(ITEM_26_JIGGY_TOTAL, 1);

        if (jiggy_marker != NULL)
        {
            Actor *jiggy_actor = marker_getActor(jiggy_marker);
            if (jiggy_actor != NULL)
            {
                chjiggy_hide(jiggy_actor);
            }
        }
    }
}

static bool prop_in_cube(Cube *c, Prop *p)
{
    if (c == NULL || p == NULL)
    {
        return FALSE;
    }

    s32 prop_index = p - c->prop2Ptr;
    if (prop_index >= 0 && prop_index < c->prop2Cnt)
    {
        return TRUE;
    }
    return FALSE;
}

static Cube *find_cube_for_prop(Prop *p)
{
    if (p == NULL)
    {
        return NULL;
    }

    for (s32 i = 0; i < sCubeList.cubeCnt; i++)
    {
        Cube *cur_cube = &sCubeList.cubes[i];
        if (prop_in_cube(cur_cube, p))
        {
            return cur_cube;
        }
    }

    if (prop_in_cube(sCubeList.unk3C, p))
    {
        return sCubeList.unk3C;
    }

    if (prop_in_cube(sCubeList.unk40, p))
    {
        return sCubeList.unk40;
    }

    return NULL;
}

typedef struct
{
    u32 note_index;
} NoteSavingExtensionData;

void collect_note(int map_id, int level_id, bool is_dynamic, int note_index)
{
    recomp_printf("[COLLECTION] collect_note called: map=%d, level=%d, is_dynamic=%d, note_index=%d\n",
                  map_id, level_id, is_dynamic, note_index);

    if (!bkrecomp_note_saving_active())
    {
        recomp_printf("[COLLECTION] Note saving not active, returning\n");
        return;
    }

    if (bkrecomp_is_note_collected(map_id, level_id, note_index))
    {
        recomp_printf("[COLLECTION] Note already collected, returning\n");
        return;
    }

    recomp_printf("[COLLECTION] Collecting note with apply_remote_state=TRUE\n");
    apply_remote_state = TRUE;
    if (is_dynamic)
    {
        recomp_printf("[COLLECTION] Calling bkrecomp_collect_dynamic_note(map=%d, level=%d)\n", map_id, level_id);
        bkrecomp_collect_dynamic_note(map_id, level_id);
        recomp_printf("[COLLECTION] Dynamic note collection complete\n");
    }
    else
    {
        recomp_printf("[COLLECTION] Calling bkrecomp_set_note_collected for static note\n");
        bkrecomp_set_note_collected(map_id, level_id, note_index);
    }
    apply_remote_state = FALSE;

    item_adjustByDiffWithoutHud(ITEM_C_NOTE, 1);

    if (!is_dynamic)
    {
        recomp_printf("[COLLECTION] Searching for note prop to despawn (note_index=%d)\n", note_index);
        for (s32 cube_idx = 0; cube_idx < sCubeList.cubeCnt; cube_idx++)
        {
            Cube *cube = &sCubeList.cubes[cube_idx];
            if (cube == NULL || cube->prop2Ptr == NULL)
            {
                continue;
            }

            for (s32 prop_idx = 0; prop_idx < cube->prop2Cnt; prop_idx++)
            {
                Prop *prop = &cube->prop2Ptr[prop_idx];

                if (prop->is_actor || prop->is_3d)
                {
                    continue;
                }

                NoteSavingExtensionData *note_data =
                    (NoteSavingExtensionData *)bkrecomp_get_extended_prop_data(cube, prop, bkrecomp_notesaving_get_note_saving_prop_extension_id());

                if (note_data != NULL && note_data->note_index == note_index)
                {
                    recomp_printf("[COLLECTION] Found note prop in main cubes, despawning\n");
                    prop->spriteProp.unk8_4 = FALSE;
                    return;
                }
            }
        }

        Cube *fallback_cubes[] = {sCubeList.unk3C, sCubeList.unk40};
        for (s32 fb_idx = 0; fb_idx < 2; fb_idx++)
        {
            Cube *cube = fallback_cubes[fb_idx];
            if (cube == NULL || cube->prop2Ptr == NULL)
            {
                continue;
            }

            for (s32 prop_idx = 0; prop_idx < cube->prop2Cnt; prop_idx++)
            {
                Prop *prop = &cube->prop2Ptr[prop_idx];

                if (prop->is_actor || prop->is_3d)
                {
                    continue;
                }

                NoteSavingExtensionData *note_data =
                    (NoteSavingExtensionData *)bkrecomp_get_extended_prop_data(cube, prop, bkrecomp_notesaving_get_note_saving_prop_extension_id());

                if (note_data != NULL && note_data->note_index == note_index)
                {
                    recomp_printf("[COLLECTION] Found note prop in fallback cubes, despawning\n");
                    prop->spriteProp.unk8_4 = FALSE;
                    return;
                }
            }
        }
    }
}

void open_level(int world_id, int jiggy_cost)
{
    enum file_progress_e level_open_flag = FILEPROG_31_MM_OPEN + (world_id - 1);

    if (fileProgressFlag_get(level_open_flag))
    {
        return;
    }

    apply_remote_state = TRUE;

    fileProgressFlag_set(level_open_flag, TRUE);

    item_adjustByDiffWithoutHud(ITEM_26_JIGGY_TOTAL, -jiggy_cost);

    apply_remote_state = FALSE;

    enum map_e current_map = map_get();
    enum level_e current_level = level_get();

    if (current_level == LEVEL_6_LAIR)
    {
        transitionToMap(current_map, -1, 0);
    }
}

void despawn_collected_items_in_map(void)
{
    if (suBaddieActorArray == NULL)
    {
        return;
    }

    enum map_e current_map = map_get();
    int despawned_honeycombs = 0;
    int despawned_tokens = 0;

    extern enum honeycomb_e func_802CA1C4(Actor * this);
    extern enum mumbotoken_e func_802E0CB0(Actor * this);

    Actor *begin = suBaddieActorArray->data;
    Actor *end = begin + suBaddieActorArray->cnt;

    int checked_actors = 0;
    for (Actor *actor = begin; actor < end; actor++)
    {
        if (actor->despawn_flag || actor->marker == NULL)
        {
            continue;
        }

        checked_actors++;
        enum actor_e actor_id = actor->modelCacheIndex;

        if (actor_id == ACTOR_47_EMPTY_HONEYCOMB || actor_id == ACTOR_50_HONEYCOMB)
        {
            enum honeycomb_e honeycomb_id = func_802CA1C4(actor);

            if (sync_is_honeycomb_collected((s16)current_map, (s16)honeycomb_id))
            {
                marker_despawn(actor->marker);
                despawned_honeycombs++;
            }
        }
        else if (actor_id == ACTOR_2D_MUMBO_TOKEN)
        {
            enum mumbotoken_e token_id = func_802E0CB0(actor);

            if (sync_is_token_collected((s16)current_map, (s16)token_id))
            {
                marker_despawn(actor->marker);
                despawned_tokens++;
            }
        }
    }

    if (despawned_honeycombs > 0 || despawned_tokens > 0)
    {
        recomp_printf("[MapLoad] Despawned %d honeycombs, %d tokens\n",
                      despawned_honeycombs, despawned_tokens);
    }
}

RECOMP_HOOK_RETURN("spawnQueue_flush")
void spawnQueue_flush_hook(void)
{
    despawn_collected_items_in_map();
}
