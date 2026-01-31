#include "modding.h"
#include "functions.h"
#include "variables.h"
#include "recomputils.h"

// used to stop hooked functions
// from running when we call them ourselves
bool applying_remote_state();

void collect_jiggy(int jiggy_enum_id, int collected_value);
void collect_note(int map_id, int level_id, bool is_dynamic, int note_index);
