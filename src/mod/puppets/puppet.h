#ifndef PUPPET_H
#define PUPPET_H

#include "modding.h"
#include "functions.h"
#include "variables.h"

#define MARKER_PUPPET 0x400
#define ACTOR_PUPPET 0x400

// Low poly Banjo model
// Pretty sure this is what ModLoader co-op uses
#define MODEL_BANJO_LOW_POLY 0x34E
#define PUPPET_MARKER_ID 0xFFF

// Define a sane maximum to prevent destruction of our innocent PCs
#define MAX_PUPPETS 16

typedef struct
{
    ActorMarker *marker;
    int player_id;
    int is_spawned;
    enum map_e last_map;
    enum level_e last_level;
    f32 last_x, last_y, last_z;
    f32 last_yaw;
    u16 last_anim;
    u32 last_update_time;
} PuppetState;

void puppet_system_init(void);

Actor *puppet_spawn(f32 position[3], f32 yaw);
void puppet_despawn(Actor *puppet);
void puppet_despawn_all(void);

void puppet_update_position(Actor *puppet, f32 position[3], f32 yaw);
void puppet_update_animation(Actor *puppet, u16 anim_id, f32 duration, f32 timer, u8 playback_type, u8 playback_direction);

u16 puppet_get_idle_anim(void);
u16 puppet_get_walk_anim(void);
u16 puppet_get_run_anim(void);
u16 puppet_get_jump_anim(void);

Actor *puppet_get_by_player_id(int player_id);
void puppet_set_player_id(Actor *puppet, int player_id);

void puppet_actor_update(Actor *this);
Actor *puppet_actor_draw(ActorMarker *marker, Gfx **gfx, Mtx **mtx, Vtx **vtx);

void puppet_handle_player_connected(int player_id);
void puppet_handle_player_disconnected(int player_id);
void puppet_update_all(void);
void puppet_send_local_state(void);
void puppet_set_spawn_delay_all(void);

#pragma pack(push, 1)
typedef struct
{
    f32 x, y, z;
    f32 yaw, pitch, roll;
    f32 anim_duration;
    f32 anim_timer;
    s16 map_id;
    s16 level_id;
    u16 anim_id;
    u8 model_id;
    u8 flags;
    u8 playback_type;
    u8 playback_direction;
} PuppetUpdateData;
#pragma pack(pop)

void puppet_handle_remote_update(int player_id, PuppetUpdateData *data);

#endif
