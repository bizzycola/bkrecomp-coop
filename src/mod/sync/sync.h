#include "modding.h"
#include "functions.h"
#include "variables.h"

#define MAX_COLLECTED_NOTES 900
#define MAX_COLLECTED_JIGGIES 90
#define MAX_COLLECTED_HONEYCOMBS 24
#define MAX_COLLECTED_TOKENS 116
#define MAX_COLLECTED_JINJOS 45

typedef struct
{
    s16 map_id;
    s16 level_id;
    bool is_dynamic;
    s16 note_index;
} NoteIdentifier;

typedef struct
{
    s16 level_id;
    s16 jiggy_id;
} JiggyIdentifier;

typedef struct
{
    s16 map_id;
    s16 actor_id;
    s16 x;
    s16 y;
    s16 z;
} ActorCollectibleIdentifier;

typedef struct
{
    NoteIdentifier collected_notes[MAX_COLLECTED_NOTES];
    int note_count;

    JiggyIdentifier collected_jiggies[MAX_COLLECTED_JIGGIES];
    int jiggy_count;

    ActorCollectibleIdentifier collected_honeycombs[MAX_COLLECTED_HONEYCOMBS];
    int honeycomb_count;

    ActorCollectibleIdentifier collected_tokens[MAX_COLLECTED_TOKENS];
    int token_count;

    ActorCollectibleIdentifier collected_jinjos[MAX_COLLECTED_JINJOS];
    int jinjo_count;
} CollectionState;

void sync_init(void);
void sync_clear(void);

void sync_add_jiggy(int jiggy_enum_id, int collected_value);
int sync_is_jiggy_collected(s16 jiggy_enum_id, s16 collected_value);
void sync_add_note(int map_id, int level_id, bool is_dynamic, int note_index);
int sync_is_note_collected(s16 map_id, s16 level_id, bool is_dynamic, s16 note_index);
