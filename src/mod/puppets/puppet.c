#include "puppet.h"
#include "recomputils.h"
#include "core2/anctrl.h"
#include "core2/modelRender.h"

RECOMP_IMPORT(".", unsigned int GetClockMS(void));

extern enum map_e map_get(void);
extern s32 level_get(void);
extern f32 player_getYaw(void);
extern AnimCtrl *baanim_getAnimCtrlPtr(void);
extern f32 sqrtf(f32);

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

typedef struct
{
    int active;
    int player_id;
    f32 position[3];
    f32 yaw;
    u16 anim_id;
    enum map_e map_id;
    enum level_e level_id;
} SimplePuppet;

static SimplePuppet s_simple_puppet[MAX_PUPPETS];
static int s_simple_puppets_initalised = 0;
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

// how frequently to update stuff
#define PUPPET_UPDATE_INTERVAL_MS 100
#define PUPPET_SPAWN_DELAY_MS 5000
#define POSITION_CHANGE_THRESHOLD 5.0f
#define YAW_CHANGE_THRESHOLD 5.0f
#define PUPPET_INTERP_SPEED 0.3f // speed for smoothing

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
    // TODO
}

// custom draw function, may ot be needed, but I'm just trying anything at this point
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

void puppet_system_init(void)
{
    // register custom actor
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
}

Actor *puppet_spanw(f32 position[3], f32 yaw)
{
    // todo
    return NULL;
}

void puppet_update_position(Actor *puppet, f32 position[3], f32 yaw)
{
    // TODO
}

static void puppet_interpolate_movement(int puppet_index)
{
    // TODO
}

static f32 puppet_get_anim_duration(enum asset_e anim_id)
{
    // Hard code speeds for animations
    // probably wrong, but will tune
    if (anim_id == ANIM_BANJO_IDLE)
        return 10.0f;
    if (anim_id == ANIM_BANJO_WALK)
        return 2.0f;
    if (anim_id == ANIM_BANJO_RUN)
        return 1.5f;
    if (anim_id == ANIM_BANJO_JUMP)
        return 3.0f;

    // Return this as a default
    return 3.0f;
}

void puppet_update_animation(Actor *puppet, u16 anim_id)
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
    if (current_anim == (enum asset_e)anim_id)
        return;

    enum asset_e asset_id = (enum asset_e)anim_id;
    f32 duration = puppet_get_anim_duration(asset_id);

    anctrl_reset(puppet->anctrl);
    anctrl_setIndex(puppet->anctrl, asset_id);
    anctrl_setDuration(puppet->anctrl, duration);
    anctrl_setPlaybackType(puppet->anctrl, ANIMCTRL_LOOP);
    anctrl_setStart(puppet->anctrl, 0.0f);
    anctrl_start(puppet->anctrl, "puppet.c", 0);
}

u16 puppet_get_idle_anim(void) { return ANIM_BANJO_IDLE; }
u16 puppet_get_walk_anim(void) { return ANIM_BANJO_WALK; }
u16 puppet_get_run_anim(void) { return ANIM_BANJO_RUN; }
u16 puppet_get_jump_anim(void) { return ANIM_BANJO_JUMP; }

void puppet_despawn(Actor *puppet)
{
    // TODO
}

void puppet_set_spawn_delay_all(void)
{
    // TODO
}

Actor *puppet_get_by_player_id(int player_id)
{
    // TODO
    return NULL;
}

void puppet_set_player_id(Actor *puppet, int player_id)
{
    // TODO
}

// Attach our actor to the actor list after it is created
RECOMP_HOOK_RETURN("spawnableActorList_new")
void puppet_register_hook(void)
{
    puppet_system_init();
}

void puppet_handle_player_connected(int player_id)
{
    // New player connected
    // TODO: create puppet
}

void puppet_handle_player_disconnected(int player_id)
{
    // A player disconnected
    // TODO: destroy innocent puppet
}

void puppet_handle_remote_update(int player_id, PuppetUpdateData *data)
{
    if (data == NULL)
        return;

    // TODO
}

void puppet_send_local_state(void)
{
    // update throttling (for the sanity of us all)
    u32 current_time = GetClockMS();
    if (current_time - s_last_puppet_send_time < PUPPET_UPDATE_INTERVAL_MS)
        return;

    f32 player_pos[3];
    player_getPosition(player_pos);

    f32 player_yaw = player_getYaw();
    enum map_e current_map = map_get();
    enum level_e current_level = level_get();

    u16 current_anim = puppet_get_idle_anim();
    AnimCtrl *player_anctrl = baanim_getAnimCtrlPtr();
    if (player_anctrl != NULL)
    {
        current_anim = (u16)anctrl_getIndex(player_anctrl);
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
    update_data.map_id = (s16)current_level;
    update_data.level_id = (s16)current_map;
    update_data.anim_id = current_anim;
    update_data.model_id = 0;
    update_data.flags = 0;

    extern int net_send_puppet_update(void *puppet_data);
    net_send_puppet_update(&update_data);
}

void puppet_update_all(void)
{
    // TODO
}
