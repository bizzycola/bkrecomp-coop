#include "modding.h"
#include "functions.h"
#include "variables.h"
#include "recomputils.h"

bool applying_remote_state();

void with_applying_remote_state(void (*fn)(void *ctx), void *ctx);

void collect_jiggy(int jiggy_enum_id, int collected_value);
void collect_note(int map_id, int level_id, bool is_dynamic, int note_index);
void open_level(int world_id, int jiggy_cost);
