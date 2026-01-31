#import "collection.h"
#include "bkrecomp_api.h"

static bool apply_remote_state = FALSE;

extern void jiggyscore_setCollected(int levelid, int jiggy_id);
extern int jiggyscore_isCollected(int levelid, int jiggy_id);
extern void item_adjustByDiffWithoutHud(int item, int diff);
extern ActorMarker *func_8032B16C(enum jiggy_e jiggy_id);
extern void marker_despawn(ActorMarker *marker);
extern void chjiggy_hide(Actor *actor);
extern Actor *marker_getActor(ActorMarker *marker);

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
    if (!bkrecomp_note_saving_active())
    {
        return;
    }

    if (bkrecomp_is_note_collected(map_id, level_id, note_index))
    {
        return;
    }

    apply_remote_state = TRUE;
    if (is_dynamic)
    {
        bkrecomp_collect_dynamic_note(map_id, level_id);
    }
    else
    {
        bkrecomp_set_note_collected(map_id, level_id, note_index);
    }
    apply_remote_state = FALSE;

    if (!is_dynamic)
    {
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
                    prop->spriteProp.unk8_4 = FALSE;
                    return;
                }
            }
        }
    }
}
