#include "recomputils.h"
#include "sync.h"
#include "../message_queue/message_queue.h"

#define POS_TOLERANCE 10

CollectionState g_collection_state = {0};

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
    g_collection_state.honeycomb_count = 0;
    g_collection_state.token_count = 0;
    g_collection_state.jinjo_count = 0;
}

void sync_clear(void)
{
    g_collection_state.note_count = 0;
    g_collection_state.jiggy_count = 0;
    g_collection_state.honeycomb_count = 0;
    g_collection_state.token_count = 0;
    g_collection_state.jinjo_count = 0;
}

static void sync_add_actor_collectible(ActorCollectibleIdentifier *arr, int *count, int maxCount,
                                      s16 map_id, s16 actor_id, s16 x, s16 y, s16 z)
{
    if (!arr || !count)
        return;

    for (int i = 0; i < *count; i++)
    {
        ActorCollectibleIdentifier *it = &arr[i];
        if (it->map_id == map_id && it->actor_id == actor_id)
        {
            return;
        }
    }

    if (*count >= maxCount)
        return;

    ActorCollectibleIdentifier *it = &arr[*count];
    it->map_id = map_id;
    it->actor_id = actor_id;
    it->x = x;
    it->y = y;
    it->z = z;

    (*count)++;
}

static int sync_is_actor_collectible_collected(const ActorCollectibleIdentifier *arr, int count,
                                               s16 map_id, s16 actor_id)
{
    if (!arr)
        return 0;

    for (int i = 0; i < count; i++)
    {
        const ActorCollectibleIdentifier *it = &arr[i];
        if (it->map_id == map_id && it->actor_id == actor_id)
        {
            return 1;
        }
    }
    return 0;
}

void sync_add_honeycomb(int map_id, int honeycomb_id, s16 x, s16 y, s16 z)
{
    sync_add_actor_collectible(g_collection_state.collected_honeycombs, &g_collection_state.honeycomb_count,
                              MAX_COLLECTED_HONEYCOMBS, (s16)map_id, (s16)honeycomb_id, x, y, z);
}

int sync_is_honeycomb_collected(s16 map_id, s16 honeycomb_id)
{
    return sync_is_actor_collectible_collected(g_collection_state.collected_honeycombs, g_collection_state.honeycomb_count,
                                               map_id, honeycomb_id);
}

void sync_add_token(int map_id, int token_id, s16 x, s16 y, s16 z)
{
    sync_add_actor_collectible(g_collection_state.collected_tokens, &g_collection_state.token_count,
                              MAX_COLLECTED_TOKENS, (s16)map_id, (s16)token_id, x, y, z);
}

int sync_is_token_collected(s16 map_id, s16 token_id)
{
    return sync_is_actor_collectible_collected(g_collection_state.collected_tokens, g_collection_state.token_count,
                                               map_id, token_id);
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

void sync_add_jiggy(int jiggy_enum_id, int collected_value)
{
    for (int i = 0; i < g_collection_state.jiggy_count; i++)
    {
        JiggyIdentifier *jiggy = &g_collection_state.collected_jiggies[i];
        if (jiggy->level_id == jiggy_enum_id && jiggy->jiggy_id == collected_value)
        {
            return;
        }
    }

    JiggyIdentifier *jiggy = &g_collection_state.collected_jiggies[g_collection_state.jiggy_count];
    jiggy->level_id = jiggy_enum_id;
    jiggy->jiggy_id = collected_value;

    g_collection_state.jiggy_count++;
}

int sync_is_jiggy_collected(s16 jiggy_enum_id, s16 collected_value)
{
    for (int i = 0; i < g_collection_state.jiggy_count; i++)
    {
        JiggyIdentifier *jiggy = &g_collection_state.collected_jiggies[i];
        if (jiggy->level_id == jiggy_enum_id && jiggy->jiggy_id == collected_value)
        {
            return 1;
        }
    }
    return 0;
}

void sync_add_note(int map_id, int level_id, bool is_dynamic, int note_index)
{
    for (int i = 0; i < g_collection_state.note_count; i++)
    {
        NoteIdentifier *note = &g_collection_state.collected_notes[i];
        if (note->map_id == map_id && note->level_id == level_id &&
            note->is_dynamic == is_dynamic && note->note_index == note_index)
        {
            return;
        }
    }

    if (g_collection_state.note_count >= MAX_COLLECTED_NOTES)
    {
        return;
    }

    NoteIdentifier *note = &g_collection_state.collected_notes[g_collection_state.note_count];
    note->map_id = map_id;
    note->level_id = level_id;
    note->is_dynamic = is_dynamic;
    note->note_index = note_index;

    g_collection_state.note_count++;
}

int sync_is_note_collected(s16 map_id, s16 level_id, bool is_dynamic, s16 note_index)
{
    for (int i = 0; i < g_collection_state.note_count; i++)
    {
        NoteIdentifier *note = &g_collection_state.collected_notes[i];
        if (note->map_id == map_id && note->level_id == level_id &&
            note->is_dynamic == is_dynamic && note->note_index == note_index)
        {
            return 1;
        }
    }
    return 0;
}
