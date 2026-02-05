#include "teleport/coop_teleport.h"
#include "../toast/toast.h"
#include "recomputils.h"
#include "functions.h"

#ifndef COOP_DEBUG_LOGS
#define COOP_DEBUG_LOGS 0
#endif

static int s_pending_teleport = 0;
static enum map_e s_pending_teleport_map = 0;
static enum level_e s_pending_teleport_level = 0;
static float s_pending_teleport_position[3] = {0.0f, 0.0f, 0.0f};
static float s_pending_teleport_yaw = 0.0f;

extern void player_setPosition(float position[3]);

int coop_teleport_is_pending(void)
{
    return s_pending_teleport;
}

void coop_teleport_set_pending(enum map_e map, enum level_e level, float position[3], float yaw)
{
    s_pending_teleport = 1;
    s_pending_teleport_map = map;
    s_pending_teleport_level = level;
    s_pending_teleport_position[0] = position[0];
    s_pending_teleport_position[1] = position[1];
    s_pending_teleport_position[2] = position[2];
    s_pending_teleport_yaw = yaw;
}

void coop_teleport_update(enum map_e current_map, enum level_e current_level)
{
    if (!s_pending_teleport)
    {
        return;
    }

    if (current_map == s_pending_teleport_map && current_level == s_pending_teleport_level)
    {
        player_setPosition(s_pending_teleport_position);

        recomp_printf("[COOP] Pending teleport applied: pos=(%.1f,%.1f,%.1f)\n",
                      s_pending_teleport_position[0], s_pending_teleport_position[1], s_pending_teleport_position[2]);

        toast_success("Teleported to player");
        s_pending_teleport = 0;
    }
    else if (COOP_DEBUG_LOGS)
    {
        recomp_printf("[COOP] Waiting for map transition: current=%d/%d, target=%d/%d\n",
                      current_map, current_level, s_pending_teleport_map, s_pending_teleport_level);
    }
}
