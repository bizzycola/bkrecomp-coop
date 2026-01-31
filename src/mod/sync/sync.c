#include "recomputils.h"
#include "sync.h"
#include "../message_queue/coop_messages.h"

// tolerance is for matching positions within this range
#define POS_TOLERANCE 10

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

void sync_init(void)
{
    g_collection_state.note_count = 0;
    g_collection_state.jiggy_count = 0;
}

void sync_clear(void)
{
    g_collection_state.note_count = 0;
    g_collection_state.jiggy_count = 0;
}

int positions_match(s16 x1, s16 y1, s16 z1, s16 x2, s16 y2, s16 z2)
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

void sync_add_jiggy(int level_id, int jiggy_id)
{
    // don't track if we already have this jiggy
    for (int i = 0; i < g_collection_state.jiggy_count; i++)
    {
        JiggyIdentifier *jiggy = &g_collection_state.collected_jiggies[i];
        if (jiggy->level_id == level_id && jiggy->jiggy_id == jiggy_id)
        {
            return;
        }
    }

    JiggyIdentifier *jiggy = &g_collection_state.collected_jiggies[g_collection_state.jiggy_count];
    jiggy->level_id = level_id;
    jiggy->jiggy_id = jiggy_id;

    g_collection_state.jiggy_count++;

    recomp_printf("got jiggy level=%d id=%d total=%d\n",
                  level_id, jiggy_id, g_collection_state.jiggy_count);
}

int sync_is_jiggy_collected(s16 level_id, s16 jiggy_id)
{
    for (int i = 0; i < g_collection_state.jiggy_count; i++)
    {
        JiggyIdentifier *jiggy = &g_collection_state.collected_jiggies[i];
        if (jiggy->level_id == level_id && jiggy->jiggy_id == jiggy_id)
        {
            return 1;
        }
    }
    return 0;
}

