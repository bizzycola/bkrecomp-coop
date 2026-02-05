#include "puppet.h"
#include "recomputils.h"
#include "core2/anctrl.h"
#include "core2/modelRender.h"
#include "console/console.h"

RECOMP_IMPORT(".", unsigned int GetClockMS(void));
RECOMP_IMPORT(".", void net_send_puppet_update(int x, int y, int z, int rot));

extern enum map_e map_get(void);
extern s32 level_get(void);
extern f32 player_getYaw(void);
extern AnimCtrl *baanim_getAnimCtrlPtr(void);
extern f32 sqrtf(f32);
extern void player_getPosition(f32 dst[3]);
extern Actor *actor_new(s32 position[3], s32 yaw, ActorInfo *actorInfo, u32 flags);
extern void marker_despawn(ActorMarker *marker);
extern void spawnableActorList_add(ActorInfo *arg0, Actor *(*arg1)(s32[3], s32, ActorInfo *, u32), u32 arg2);
extern Actor *marker_getActor(ActorMarker *marker);
extern Actor *actor_draw(ActorMarker *marker, Gfx **gfx, Mtx **mtx, Vtx **vtx);
extern void actor_update_func_80326224(Actor *this);
extern void transitionToMap(enum map_e map, s32 exit, s32 transition);

static ActorAnimationInfo puppet_anim_table[] = {
    {0x0000, 0.0f},       // no anim
    {0x0003, 1000000.0f}, // walking
    {0x0001, 1000000.0f}  // standing idle
};

extern Actor *
actor_draw(ActorMarker *marker, Gfx **gfx, Mtx **mtx, Vtx **vtx);
extern Actor *marker_getActor(ActorMarker *marker);

#define ANIM_BANJO_IDLE ASSET_6F_ANIM_BSSTAND_IDLE
#define ANIM_BANJO_WALK ASSET_3_ANIM_BSWALK
#define ANIM_BANJO_RUN ASSET_C_ANIM_BSWALK_RUN
#define ANIM_BANJO_JUMP ASSET_8_ANIM_BSJUMP

static ActorInfo s_puppetActorInfo;
static int s_puppet_registered = 0;
static BKModelBin *s_puppetModel = NULL;

static PuppetState s_puppets[MAX_PUPPETS];
static int s_puppets_initialized = 0;

typedef struct
{
    f32 target_pos[3];
    f32 target_yaw;
    u16 target_anim;
    f32 target_anim_duration;
    f32 target_anim_timer;
    u8 target_playback_type;
    u8 target_playback_direction;
    int has_target;
    u32 last_update_time;
} PuppetInterpolation;

static PuppetInterpolation s_puppet_interp[MAX_PUPPETS];

static u32 s_last_puppet_send_time = 0;

static struct
{
    f32 last_pos[3];
    f32 last_yaw;
    u16 last_anim;
    s16 last_map;
    s16 last_level;
    int initialized;
} s_local_puppet_cache = {0};

#define PUPPET_UPDATE_INTERVAL_MS 100
#define PUPPET_SPAWN_DELAY_MS 5000
#define POSITION_CHANGE_THRESHOLD 5.0f
#define YAW_CHANGE_THRESHOLD 5.0f
#define PUPPET_INTERP_SPEED 0.3f
#define ANIM_TIMER_INTERP_SPEED 0.15f

static f32 lerp_f32(f32 from, f32 to, f32 alpha)
{
    return from + (to - from) * alpha;
}

static f32 lerp_angle(f32 from, f32 to, f32 alpha)
{
    f32 diff = to - from;

    while (diff > 180.0f)
        diff -= 360.0f;
    while (diff < -180.0f)
        diff += 360.0f;

    return from + diff * alpha;
}

void puppet_actor_update(Actor *this)
{
    if (this == NULL || !this->initialized)
        return;

    int puppet_index = -1;
    for (int i = 0; i < MAX_PUPPETS; i++)
    {
        if (s_puppets[i].is_spawned && s_puppets[i].marker != NULL && s_puppets[i].marker == this->marker)
        {
            puppet_index = i;
            break;
        }
    }

    if (puppet_index == -1)
    {
        return;
    }

    PuppetInterpolation *interp = &s_puppet_interp[puppet_index];
    if (interp->has_target)
    {
        // Smooth position interpolation
        this->position[0] = lerp_f32(this->position[0], interp->target_pos[0], PUPPET_INTERP_SPEED);
        this->position[1] = lerp_f32(this->position[1], interp->target_pos[1], PUPPET_INTERP_SPEED);
        this->position[2] = lerp_f32(this->position[2], interp->target_pos[2], PUPPET_INTERP_SPEED);
        this->yaw = lerp_angle(this->yaw, interp->target_yaw, PUPPET_INTERP_SPEED);

        if (this->anctrl != NULL)
        {
            int is_idle = (interp->target_anim == ANIM_BANJO_IDLE);

            if (is_idle)
            {
                enum asset_e current_anim = anctrl_getIndex(this->anctrl);
                if (current_anim != ANIM_BANJO_IDLE)
                {
                    anctrl_reset(this->anctrl);
                    anctrl_setIndex(this->anctrl, ANIM_BANJO_IDLE);
                    anctrl_setDuration(this->anctrl, 15.0f);
                    anctrl_setPlaybackType(this->anctrl, ANIMCTRL_LOOP);
                    anctrl_setDirection(this->anctrl, 1);
                    anctrl_setStart(this->anctrl, 0.0f);
                    anctrl_start(this->anctrl, "puppet.c", 0);
                }
                anctrl_update(this->anctrl);
            }
            else if (interp->target_anim != 0)
            {
                puppet_update_animation(this, interp->target_anim, interp->target_anim_duration,
                                        interp->target_anim_timer, interp->target_playback_type,
                                        interp->target_playback_direction);
            }
        }
    }
}

Actor *puppet_actor_draw(ActorMarker *marker, Gfx **gfx, Mtx **mtx, Vtx **vtx)
{
    if (marker == NULL)
        return NULL;

    Actor *actor = marker_getActor(marker);
    if (actor == NULL)
        return NULL;

    if (!actor->initialized)
    {
        return actor;
    }

    if (s_puppetModel != NULL)
    {
        f32 position[3];
        position[0] = actor->position[0];
        position[1] = actor->position[1];
        position[2] = actor->position[2];

        f32 rotation[3];
        rotation[0] = 0.0f;
        rotation[1] = actor->yaw;
        rotation[2] = 0.0f;

        modelRender_draw(gfx, mtx, position, rotation, 1.0f, NULL, s_puppetModel);
        return actor;
    }

    return actor_draw(marker, gfx, mtx, vtx);
}

Actor *puppet_spawn(f32 position[3], f32 yaw)
{
    if (!s_puppet_registered)
    {
        return NULL;
    }

    if (position[0] < -15000.0f || position[1] < -15000.0f || position[2] < -15000.0f)
    {
        return NULL;
    }

    s32 pos_int[3];
    pos_int[0] = (s32)position[0];
    pos_int[1] = (s32)position[1];
    pos_int[2] = (s32)position[2];

    Actor *puppet = actor_new(pos_int, (s32)yaw, &s_puppetActorInfo, 0);
    if (puppet != NULL)
    {
        puppet->position[0] = position[0];
        puppet->position[1] = position[1];
        puppet->position[2] = position[2];
        puppet->yaw = yaw;
        puppet->initialized = TRUE;

        puppet->anctrl = anctrl_new(0);
        if (puppet->anctrl != NULL)
        {
            anctrl_reset(puppet->anctrl);
            anctrl_setIndex(puppet->anctrl, ANIM_BANJO_IDLE);
            anctrl_setDuration(puppet->anctrl, 15.0f);
            anctrl_setPlaybackType(puppet->anctrl, ANIMCTRL_LOOP);
            anctrl_setStart(puppet->anctrl, 0.0f);
            anctrl_start(puppet->anctrl, "puppet.c", 0);
        }
    }

    return puppet;
}

void puppet_update_position(Actor *puppet, f32 position[3], f32 yaw)
{
    if (puppet == NULL || !puppet->initialized || puppet->marker == NULL)
    {
        return;
    }

    int puppet_index = -1;
    for (int i = 0; i < MAX_PUPPETS; i++)
    {
        if (s_puppets[i].is_spawned && s_puppets[i].marker == puppet->marker)
        {
            puppet_index = i;
            break;
        }
    }

    if (puppet_index == -1)
    {
        return;
    }

    PuppetInterpolation *interp = &s_puppet_interp[puppet_index];
    interp->target_pos[0] = position[0];
    interp->target_pos[1] = position[1];
    interp->target_pos[2] = position[2];
    interp->target_yaw = yaw;
    interp->has_target = 1;
    interp->last_update_time = GetClockMS();
}

void puppet_update_animation(Actor *puppet, u16 anim_id, f32 duration, f32 timer, u8 playback_type, u8 playback_direction)
{
    if (puppet == NULL)
        return;
    if (!puppet->initialized)
        return;
    if (puppet->anctrl == NULL)
        return;
    if (puppet->marker == NULL)
        return;

    enum asset_e current_anim = anctrl_getIndex(puppet->anctrl);
    int anim_changed = (current_anim != (enum asset_e)anim_id);

    enum asset_e asset_id = (enum asset_e)anim_id;

    if (duration < 0.01f)
        duration = 0.01f;
    if (duration > 100.0f)
        duration = 100.0f;

    if (playback_type < 1 || playback_type > 4)
        playback_type = ANIMCTRL_LOOP;

    if (anim_changed)
    {
        anctrl_reset(puppet->anctrl);
        anctrl_setIndex(puppet->anctrl, asset_id);
        anctrl_setDuration(puppet->anctrl, duration);
        anctrl_setPlaybackType(puppet->anctrl, (enum anctrl_playback_e)playback_type);
        anctrl_setDirection(puppet->anctrl, playback_direction);
        anctrl_setAnimTimer(puppet->anctrl, timer);
        anctrl_setStart(puppet->anctrl, 0.0f);
        anctrl_start(puppet->anctrl, "puppet.c", 0);
    }
    else
    {
        anctrl_setDuration(puppet->anctrl, duration);
        anctrl_setPlaybackType(puppet->anctrl, (enum anctrl_playback_e)playback_type);
        anctrl_setDirection(puppet->anctrl, playback_direction);

        f32 current_timer = anctrl_getAnimTimer(puppet->anctrl);
        f32 timer_diff = timer - current_timer;

        if (playback_type == ANIMCTRL_LOOP)
        {
            if (timer_diff < -0.5f)
            {
                timer_diff += 1.0f;
            }
            else if (timer_diff > 0.5f)
            {
                timer_diff -= 1.0f;
            }
        }

        f32 new_timer = current_timer + timer_diff * ANIM_TIMER_INTERP_SPEED;

        if (new_timer < 0.0f)
            new_timer += 1.0f;
        if (new_timer > 1.0f)
            new_timer -= 1.0f;

        anctrl_setAnimTimer(puppet->anctrl, new_timer);

        anctrl_update(puppet->anctrl);
    }
}

u16 puppet_get_idle_anim(void) { return ANIM_BANJO_IDLE; }
u16 puppet_get_walk_anim(void) { return ANIM_BANJO_WALK; }
u16 puppet_get_run_anim(void) { return ANIM_BANJO_RUN; }
u16 puppet_get_jump_anim(void) { return ANIM_BANJO_JUMP; }

void puppet_despawn(Actor *puppet)
{
    if (puppet == NULL)
        return;

    for (int i = 0; i < MAX_PUPPETS; i++)
    {
        if (s_puppets[i].is_spawned && s_puppets[i].marker != NULL && s_puppets[i].marker == puppet->marker)
        {
            s_puppets[i].is_spawned = 0;
            s_puppets[i].marker = NULL;
            s_puppet_interp[i].has_target = 0;
            break;
        }
    }

    if (puppet->marker != NULL)
    {
        marker_despawn(puppet->marker);
    }
}

void puppet_despawn_all(void)
{
    for (int i = 0; i < MAX_PUPPETS; i++)
    {
        if (s_puppets[i].is_spawned && s_puppets[i].marker != NULL)
        {
            Actor *actor = marker_getActor(s_puppets[i].marker);
            if (actor != NULL)
            {
                puppet_despawn(actor);
            }
            else
            {
                s_puppets[i].is_spawned = 0;
                s_puppets[i].marker = NULL;
                s_puppet_interp[i].has_target = 0;
            }
        }
    }
}
int puppet_despawn_all_cmd(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    puppet_despawn_all();
    console_log_success("All puppets despawned");

    return 1;
}

void puppet_set_spawn_delay_all(void)
{
    u32 current_time = GetClockMS();
    for (int i = 0; i < MAX_PUPPETS; i++)
    {
        if (s_puppets[i].is_spawned)
        {
            s_puppets[i].last_update_time = current_time;
        }
    }
}

Actor *puppet_get_by_player_id(int player_id)
{
    for (int i = 0; i < MAX_PUPPETS; i++)
    {
        if (s_puppets[i].is_spawned && s_puppets[i].player_id == player_id && s_puppets[i].marker != NULL)
        {
            return marker_getActor(s_puppets[i].marker);
        }
    }
    return NULL;
}

void puppet_set_player_id(Actor *puppet, int player_id)
{
    if (puppet == NULL || puppet->marker == NULL)
        return;

    for (int i = 0; i < MAX_PUPPETS; i++)
    {
        if (s_puppets[i].is_spawned && s_puppets[i].marker == puppet->marker)
        {
            s_puppets[i].player_id = player_id;
            break;
        }
    }
}

RECOMP_HOOK_RETURN("spawnableActorList_new")
void puppet_register_hook(void)
{
    puppet_system_init();
}

void puppet_handle_player_connected(int player_id)
{
    if (!s_puppets_initialized)
    {
        for (int i = 0; i < MAX_PUPPETS; i++)
        {
            s_puppets[i].marker = NULL;
            s_puppets[i].player_id = -1;
            s_puppets[i].is_spawned = 0;
            s_puppets[i].last_update_time = 0;
            s_puppet_interp[i].has_target = 0;
        }
        s_puppets_initialized = 1;
    }

    for (int i = 0; i < MAX_PUPPETS; i++)
    {
        if (!s_puppets[i].is_spawned)
        {
            s_puppets[i].player_id = player_id;
            s_puppets[i].is_spawned = 0;
            s_puppets[i].marker = NULL;
            s_puppets[i].last_update_time = GetClockMS();
            break;
        }
    }
}

void puppet_handle_player_disconnected(int player_id)
{
    for (int i = 0; i < MAX_PUPPETS; i++)
    {
        if (s_puppets[i].player_id == player_id)
        {
            if (s_puppets[i].is_spawned && s_puppets[i].marker != NULL)
            {
                marker_despawn(s_puppets[i].marker);
            }

            s_puppets[i].marker = NULL;
            s_puppets[i].is_spawned = 0;
            s_puppets[i].player_id = -1;
            s_puppets[i].last_update_time = 0;
            s_puppet_interp[i].has_target = 0;
            break;
        }
    }
}

void puppet_handle_remote_update(int player_id, PuppetUpdateData *data)
{
    if (data == NULL)
    {
        return;
    }

    if (!s_puppets_initialized)
    {
        for (int i = 0; i < MAX_PUPPETS; i++)
        {
            s_puppets[i].marker = NULL;
            s_puppets[i].player_id = -1;
            s_puppets[i].is_spawned = 0;
            s_puppets[i].last_update_time = 0;
            s_puppet_interp[i].has_target = 0;
        }
        s_puppets_initialized = 1;
    }

    enum map_e current_map = map_get();
    enum level_e current_level = level_get();

    int puppet_index = -1;
    for (int i = 0; i < MAX_PUPPETS; i++)
    {
        if (s_puppets[i].player_id == player_id)
        {
            puppet_index = i;
            break;
        }
    }

    if ((enum map_e)data->map_id != current_map || (enum level_e)data->level_id != current_level)
    {
        if (puppet_index != -1 && s_puppets[puppet_index].is_spawned)
        {
            Actor *puppet = s_puppets[puppet_index].marker != NULL ? marker_getActor(s_puppets[puppet_index].marker) : NULL;
            if (puppet != NULL)
            {
                puppet_despawn(puppet);
            }
            else
            {
                s_puppets[puppet_index].marker = NULL;
                s_puppets[puppet_index].is_spawned = 0;
                s_puppets[puppet_index].player_id = -1;
            }
        }
        return;
    }

    if (puppet_index == -1)
    {
        for (int i = 0; i < MAX_PUPPETS; i++)
        {
            if (s_puppets[i].player_id == -1)
            {
                puppet_index = i;
                s_puppets[i].player_id = player_id;
                s_puppets[i].is_spawned = 0;
                s_puppets[i].marker = NULL;
                break;
            }
        }
    }

    if (puppet_index == -1)
    {
        return;
    }

    Actor *puppet = s_puppets[puppet_index].marker != NULL ? marker_getActor(s_puppets[puppet_index].marker) : NULL;

    if (puppet == NULL || !s_puppets[puppet_index].is_spawned)
    {
        f32 position[3] = {data->x, data->y, data->z};
        puppet = puppet_spawn(position, data->yaw);
        if (puppet != NULL)
        {
            s_puppets[puppet_index].marker = puppet->marker;
            s_puppets[puppet_index].is_spawned = 1;
        }
        else
        {
            s_puppets[puppet_index].player_id = -1;
            return;
        }
    }

    f32 position[3];
    position[0] = data->x;
    position[1] = data->y;
    position[2] = data->z;

    puppet_update_position(puppet, position, data->yaw);

    PuppetInterpolation *interp = &s_puppet_interp[puppet_index];
    interp->target_anim = data->anim_id;
    interp->target_anim_duration = data->anim_duration;
    interp->target_anim_timer = data->anim_timer;
    interp->target_playback_type = data->playback_type;
    interp->target_playback_direction = data->playback_direction;

    s_puppets[puppet_index].last_update_time = GetClockMS();
    s_puppets[puppet_index].last_map = (enum map_e)data->map_id;
    s_puppets[puppet_index].last_level = (enum level_e)data->level_id;
}

RECOMP_HOOK("transitionToMap")
void on_transition_to_map(enum map_e map, s32 exit, s32 transition)
{
    for (int i = 0; i < MAX_PUPPETS; i++)
    {
        s_puppets[i].is_spawned = 0;
        s_puppets[i].marker = NULL;
        s_puppet_interp[i].has_target = 0;
    }
    s_local_puppet_cache.initialized = 0;
}

void puppet_send_local_state(void)
{
    u32 current_time = GetClockMS();
    if (current_time - s_last_puppet_send_time < PUPPET_UPDATE_INTERVAL_MS)
        return;

    enum map_e current_map = map_get();
    enum level_e current_level = level_get();

    if (current_map <= 0 || current_map > 0x90 || current_level < 0 || current_level > 20)
    {
        return;
    }

    f32 player_pos[3];
    player_getPosition(player_pos);

    f32 player_yaw = player_getYaw();

    u16 current_anim = puppet_get_idle_anim();
    f32 current_anim_duration = 1.0f;
    f32 current_anim_timer = 0.0f;
    u8 current_playback_type = ANIMCTRL_LOOP;
    u8 current_playback_direction = 1;

    AnimCtrl *player_anctrl = baanim_getAnimCtrlPtr();
    if (player_anctrl != NULL)
    {
        current_anim = (u16)anctrl_getIndex(player_anctrl);
        current_anim_duration = anctrl_getDuration(player_anctrl);
        current_anim_timer = anctrl_getAnimTimer(player_anctrl);
        current_playback_type = (u8)anctrl_getPlaybackType(player_anctrl);
        current_playback_direction = (u8)anctrl_isPlayedForwards(player_anctrl);
    }

    if (s_local_puppet_cache.initialized)
    {
        f32 dx = player_pos[0] - s_local_puppet_cache.last_pos[0];
        f32 dy = player_pos[1] - s_local_puppet_cache.last_pos[1];
        f32 dz = player_pos[2] - s_local_puppet_cache.last_pos[2];
        f32 distance = sqrtf(dx * dx + dy * dy + dz * dz);

        f32 yaw_delta = player_yaw - s_local_puppet_cache.last_yaw;
        if (yaw_delta > 180.0f)
            yaw_delta -= 360.0f;
        if (yaw_delta < -180.0f)
            yaw_delta += 360.0f;
        yaw_delta = (yaw_delta < 0) ? -yaw_delta : yaw_delta;

        int position_changed = (distance > POSITION_CHANGE_THRESHOLD);
        int yaw_changed = (yaw_delta > YAW_CHANGE_THRESHOLD);
        int anim_changed = (current_anim != s_local_puppet_cache.last_anim);
        int map_changed = ((s16)current_level != s_local_puppet_cache.last_map ||
                           (s16)current_map != s_local_puppet_cache.last_level);

        if (!position_changed && !yaw_changed && !anim_changed && !map_changed)
            return;
    }

    s_local_puppet_cache.last_pos[0] = player_pos[0];
    s_local_puppet_cache.last_pos[1] = player_pos[1];
    s_local_puppet_cache.last_pos[2] = player_pos[2];
    s_local_puppet_cache.last_yaw = player_yaw;
    s_local_puppet_cache.last_anim = current_anim;
    s_local_puppet_cache.last_map = (s16)current_level;
    s_local_puppet_cache.last_level = (s16)current_map;
    s_local_puppet_cache.initialized = 1;
    s_last_puppet_send_time = current_time;

    PuppetUpdateData update_data;
    update_data.x = player_pos[0];
    update_data.y = player_pos[1];
    update_data.z = player_pos[2];
    update_data.yaw = player_yaw;
    update_data.pitch = 0.0f;
    update_data.roll = 0.0f;
    update_data.map_id = (s16)current_map;
    update_data.level_id = (s16)current_level;
    update_data.anim_id = current_anim;
    update_data.anim_duration = current_anim_duration;
    update_data.anim_timer = current_anim_timer;
    update_data.playback_type = current_playback_type;
    update_data.playback_direction = current_playback_direction;
    update_data.model_id = 0;
    update_data.flags = 0;

    net_send_puppet_update((int)&update_data, 0, 0, 0);
}

void puppet_update_all(void)
{
    if (!s_puppets_initialized)
        return;

    u32 current_time = GetClockMS();
    enum map_e current_map = map_get();
    enum level_e current_level = level_get();

    for (int i = 0; i < MAX_PUPPETS; i++)
    {
        if (s_puppets[i].is_spawned && s_puppets[i].marker != NULL)
        {
            Actor *puppet = marker_getActor(s_puppets[i].marker);
            if (puppet == NULL)
            {
                s_puppets[i].marker = NULL;
                s_puppets[i].is_spawned = 0;
                s_puppets[i].player_id = -1;
            }
            else if (s_puppets[i].last_map != current_map || s_puppets[i].last_level != current_level)
            {
                puppet_despawn(puppet);
            }
            else if (current_time - s_puppets[i].last_update_time > 10000)
            {
                puppet_despawn(puppet);
            }
        }
    }
}

static int puppet_cmd_init = 0;
void puppet_system_init(void)
{
    for (int i = 0; i < MAX_PUPPETS; i++)
    {
        s_puppets[i].marker = NULL;
        s_puppets[i].is_spawned = 0;
        s_puppets[i].last_update_time = 0;
        s_puppet_interp[i].has_target = 0;
    }

    s_puppetActorInfo.markerId = MARKER_PUPPET;
    s_puppetActorInfo.actorId = ACTOR_PUPPET;
    s_puppetActorInfo.modelId = MODEL_BANJO_LOW_POLY;
    s_puppetActorInfo.startAnimation = 0;
    s_puppetActorInfo.animations = puppet_anim_table;
    s_puppetActorInfo.update_func = puppet_actor_update;
    s_puppetActorInfo.update2_func = actor_update_func_80326224;
    s_puppetActorInfo.draw_func = actor_draw;
    s_puppetActorInfo.unk18 = 0;
    s_puppetActorInfo.draw_distance = 0;
    s_puppetActorInfo.shadow_scale = 0.0f;
    s_puppetActorInfo.unk20 = 0;

    spawnableActorList_add(&s_puppetActorInfo, actor_new, 0);
    s_puppet_registered = 1;

    if (!puppet_cmd_init)
    {
        console_register_command("puppet_despawn_all", puppet_despawn_all_cmd, "Despawn all puppets");
    }
    puppet_cmd_init = 1;
}