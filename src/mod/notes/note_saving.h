#ifndef __NOTE_SAVING_H__
#define __NOTE_SAVING_H__

#include "patches.h"
#include "prop.h"
#include "bk_api.h"
#include "core2/file.h"

void init_note_saving();
void calculate_map_start_note_indices();

// Notes are always saved, but this function controls whether to use the saved data to prevent notes from spawning and adjust the note score. 
bool note_saving_enabled();

void note_saving_on_map_load();
void note_saving_update();
void note_saving_handle_static_note(Cube *c, Prop *p);
void note_saving_handle_dynamic_note(Actor *actor, ActorMarker *marker);
void calculate_map_start_note_indices();
void note_saving_handle_static_note(Cube *c, Prop *p);
extern void func_8032DE78(SpriteProp *sprite_prop, enum asset_e *sprite_id_ptr);

void set_note_collected(enum map_e map_id, enum level_e level_id, u8 note_index);
bool is_note_collected(enum map_e map_id, enum level_e level_id, u8 note_index);
void collect_dynamic_note(enum map_e map_id, enum level_e level_id);

Cube *find_cube_for_prop(Prop *p);


typedef struct {
    u32 note_index;
} NoteSavingExtensionData;

// Accessor function to get the prop extension ID
PropExtensionId get_note_saving_prop_extension_id();

#endif