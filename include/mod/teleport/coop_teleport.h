#ifndef COOP_TELEPORT_H
#define COOP_TELEPORT_H

#include "modding.h"
#include "functions.h"

void coop_teleport_update(enum map_e current_map, enum level_e current_level);
int coop_teleport_is_pending(void);
void coop_teleport_set_pending(enum map_e map, enum level_e level, float position[3], float yaw);

#endif
